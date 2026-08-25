# Handoff: Native ZMQ (ZMTP) framing for the libbitcoin socket variant

Design-session handoff. No code was written; this document captures the agreed
plan, the compat facts verified against primary sources, and the scope
boundaries, so implementation can start without re-deriving any of it.

Repository layering used throughout:

- **bn** (libbitcoin-network): transport, framing, sockets, sessions,
  connectors/acceptors, peer protocols. All work below is bn unless stated.
- **bs** (libbitcoin-server): application-layer services provisioned over bn
  (electrum, rpc, ws, the zmq notification service, stratum, provisioning
  concerns).

Goal: bitcoind-compatible ZMQ notification support with **no libzmq
dependency** — native ZMTP 3.x over the existing bn socket, with CurveZMQ
(CURVE mechanism) as a later phase.

---

## 1. Architecture decision

ZMTP is implemented as a **stream type in the socket variant**, mirroring
`privacy::stream` (bip324/p2ps), not inline framing (the `socket_peer.cpp`
pattern) and not the WS path.

- New `zmq/` directory in bn mirroring `privacy/`: `context.hpp`,
  `stream.hpp` (later `cipher.hpp` for CURVE).
- `socket::context` variant already reserves the slot:
  `// TODO: zmq::context.` at `include/bitcoin/network/net/socket.hpp:48`.
  `zmq::context` holds mechanism selection (NULL now, CURVE later) and, later,
  the CURVE server keypair — exactly as `privacy::context` holds keys.
- `socket::do_handshake` (`src/net/socket_connect.cpp`) gains a third branch:
  `holds_alternative<ref<const zmq::context>>` → move the base socket into a
  `zmq::stream`, `async_handshake` (greeting + mechanism exchange).
  Inbound/outbound maps to the ZMTP as-server flag as bip324 maps
  initiator/responder.
- `socket_t` gains the `zmq::stream` alternative (cost: every `std::visit`
  grows a case — same price paid for `privacy::stream`).
- New `socket_zmq.cpp` with `zmq_read`/`zmq_write` following the
  `ws_read`/`ws_write` dispatch pattern. Write API is whole-message
  (frame list), keeping multipart atomicity inside the stream.
- **Framing/mechanism split** inside `zmq::stream`: the pump parses
  frames/commands; the mechanism (NULL = passthrough, CURVE = box/unbox)
  processes them. This is what makes CURVE a drop-in later.
- No detection/multiplexing: zmq endpoints are dedicated binds. The acceptor
  constructs sockets with a zmq context in `socket::parameters`; the handshake
  branch does the rest.
- Composition note: because the upgrade is installed by `do_handshake` and
  proxied connections defer handshake, an outbound zmq connection through
  `connector_socks` (subscriber role, e.g. to an onion endpoint) works by
  construction. Not a goal; falls out free.

The bs side owns the **notification service**: topics, per-(topic × endpoint)
sequence counters, wiring to block/tx events, bind configuration. bn owns the
wire.

## 2. bitcoind compat surface (verified in Core master `src/zmq/`)

Socket/options behavior:

- One `ZMQ_PUB` socket per distinct endpoint, `zmq_bind`. Notifiers for
  different topics sharing an address share one socket (multimap by address);
  only the first notifier's HWM is applied to a shared socket (quirk — do not
  replicate faithfully). Same topic may bind multiple addresses, each with an
  independent sequence counter.
- `ZMQ_SNDHWM` default 1000, per-topic option; `ZMQ_TCP_KEEPALIVE=1`;
  `ZMQ_IPV6` only when the bind address is IPv6; `ZMQ_LINGER=0` at shutdown.
- A notifier whose send hard-fails is dropped for the session.

Message format — three frames `[topic][body][4-byte LE sequence]`:

- Sequence is a memory-only per-notifier (per topic × endpoint) counter,
  incremented after send, starting at 0.
- All hashes byte-reversed (RPC display order).
- `rawtx` serialized with witness; `rawblock` full block serialization.
- `sequence` body: 32-byte reversed hash + label, where `C`/`D` = block
  connect/disconnect (no trailing sequence), `A`/`R` = mempool accept/removal
  with 8-byte LE mempool sequence appended.

Trigger semantics (the part compat implementations get wrong):

- `hashblock`/`rawblock`: `UpdatedBlockTip` only — suppressed during IBD and
  when tip did not advance (pure disconnects publish nothing here). With
  assumeutxo, not issued for background-chainstate historical blocks.
- `sequence`: every block connect and disconnect, plus mempool
  acceptance/removal (removal for non-block-inclusion reasons).
- `hashtx`/`rawtx`: mempool acceptance AND every transaction of every
  connected AND disconnected block — duplicates are documented behavior.

## 3. ZMTP wire requirements (verified in libzmq master)

Greeting:

- libzmq stages its greeting (10-byte signature at connect, rest incrementally
  after reading the peer's). A server that waits for a full greeting before
  writing deadlocks. **Send our complete 64-byte greeting eagerly on accept**;
  read the peer's progressively (signature + major first, then remaining 53).
- Peer validity gates: byte 0 = `0xff`, byte 9 bit 0 set. Send revision 3,
  minor 1. Accept major >= 3; refuse below (Core requires libzmq >= 4.0, so
  no pre-3.0 peers exist in practice). Unknown higher revisions are treated by
  libzmq as 3.1 — do the same.
- Peer minor selects the subscription dialect (store per connection):
  - **3.0**: subscriptions arrive as ordinary messages, first byte `\x01`
    (subscribe) / `\x00` (cancel).
  - **3.1+**: SUBSCRIBE/CANCEL commands (libzmq's v3.1 encoder converts).
  Both are mandatory.
- Mechanism field must byte-match `"NULL"` + 16 zero bytes (libzmq hard-fails
  otherwise). As-server byte = 0 for NULL.
- Bound the whole handshake with a deadline (peer default `ZMQ_HANDSHAKE_IVL`
  is 30s).

NULL handshake and commands:

- One `READY` command each way (`\x05READY` + metadata triples:
  name-len/name/value-len(u32BE)/value).
- Advertise `Socket-Type: PUB` (SUB peers accept PUB or XPUB; PUB is what
  bitcoind advertises and the safest for non-libzmq clients). Validate the
  peer's Socket-Type is SUB or XSUB; reject via `ERROR` command
  (`\x05ERROR` + length-prefixed reason).
- Peer READY carries `Socket-Type: SUB`; Identity is only attached for
  REQ/DEALER/ROUTER, so typically absent. Tolerate unknown metadata
  properties and unknown commands silently (libzmq does).
- **PING → PONG is mandatory**: PING is 2-byte TTL + up-to-16-byte context;
  reply PONG echoing context (truncate to 16). libzmq peers with heartbeats
  configured disconnect on missing PONG. Handle in the read pump (analog of
  WS control-frame absorption).

Framing:

- Frame = flags byte (bit0 MORE, bit1 LONG, bit2 COMMAND) + 1-byte or 8-byte
  (network order) length + body. v3.1 encoder for send; v2 decoder semantics
  with a message-size maximum for receive.

PUB semantics:

- Everything inbound after READY is subscription, cancel, ping, or ignorable —
  the read pump is small. PUB discards other inbound messages but must read
  continuously (subscriptions, pings, disconnect detection).
- Per-connection prefix-match subscription set (topics are few; simple
  structure suffices).
- Slow subscriber: per-connection bounded queue, **drop-new on overflow,
  silently, per-subscriber**; multipart stays atomic (libzmq rolls back
  partial multiparts). Never disconnect on overflow. Gap detection is what
  the LE32 sequence frame is for.
- Keepalive maps to `socket_base::keep_alive` on accepted sockets.

## 4. gozmq (lnd) is a first-class interop target

lnd does not use libzmq; its client is lightninglabs/gozmq, a minimal Go ZMTP
implementation:

- Sends greeting version **3.0**, NULL; errors if peer major < 3 (our 3.1
  greeting is fine — it checks only `greet[10] >= 3`).
- Subscriptions as `\x01`-prefixed messages only (never commands).
- No PING/PONG at all (relies on read timeouts and reconnect).
- Dials tcp and unix/ipc.

Consequence: the 3.0 message-form subscription path is the lnd path — test
against gozmq specifically, not just libzmq. PING/PONG matters only for
libzmq-based subscribers with heartbeats enabled.

## 5. Decisions log

- Native ZMTP; no libzmq/czmq dependency anywhere.
- Stream-variant architecture per §1 (not inline framing).
- Advertise `PUB` (XPUB-shaped internally — subscriptions parsed regardless).
- Send full greeting eagerly on accept; refuse ZMTP < 3.0.
- Both subscription dialects, selected by negotiated minor.
- PONG replies in the stream read pump.
- Drop-new bounded per-connection queues; message-count HWM knob per
  endpoint in native settings (bitcoind spirit: default 1000).
- Sequence counters per (topic × endpoint) in the bs service.
- Trigger semantics reproduce bitcoind's quirks exactly (§2).
- tcp binds only in phase 1. **No ipc/unix for zmq**: survey found no
  consumer requiring it (Core added `unix:` zmq endpoints in v28.0 for lnd,
  which defaults to tcp; nothing else supports it). No perf case (loopback
  delta is microseconds; nothing latency-critical consumes this interface).
- No bitcoind config-string parsing — compat is wire-level only; endpoint
  settings are native libbitcoin forms.
- Phases: (1) greeting + NULL + PUB semantics + both sub dialects + PING/PONG;
  (2) CURVE mechanism. CURVE = libzmq-compatible crypto_box: X25519 +
  XSalsa20-Poly1305 — primitives not currently in system (bip324 uses
  ChaCha20-Poly1305; EC is secp256k1). XSalsa20 is a contained addition to
  the cipher layer (Salsa/ChaCha family + HSalsa20 nonce extension), no new
  dependency, but real work — hence phase 2. CURVE handshake =
  HELLO/WELCOME/INITIATE/READY commands, then messages inside encrypted
  MESSAGE commands; the framing/mechanism split accommodates it without
  touching the pump.

## 6. Secondary work item: unix domain sockets (severable)

Not part of the zmq track; the zmq binds would consume it later through the
same seams. Scope as settled:

- **Base plumbing**: `socket_t` base becomes
  `tcp::socket` / `local::stream_protocol::socket`; matching local acceptor.
  ssl/ws upgrades compose over local sockets as-is; with p2ps outbound in
  scope, `privacy::stream` and v1/v2 detection must also ride the generalized
  base (do not special-case them tcp-only). Socket options branch
  (keepalive/nodelay/v6-only are tcp-only). Acceptor owns
  unlink-before-bind and unlink-on-close (asio does not; stale-file
  EADDRINUSE otherwise). Permissions via umask. Guard with
  `BOOST_ASIO_HAS_LOCAL_SOCKETS`.
- **Config**: native pathed-endpoint form, explicit scheme/prefix (never
  sniffed), validated against sun_path length. Abstract namespace (`@`) and
  wildcards excluded.
- **Incoming**: universal — every acceptor-based service (p2p inbound
  included) via acceptor-factory dispatch. Secure variants over unix are
  allowed but hollow (TLS adds nothing over file permissions); plaintext
  http/ws/electrum over unix is the strongest use (reverse-proxy upstream
  pattern).
- **Outgoing**: manual/configured p2p and p2ps peers, plus the socks proxy
  dial (one branch in `connector_socks`; negotiation and deferred handshake
  untouched). Self-enforcing beyond that: unix paths cannot enter gossip
  (no BIP155 network id), hosts, or advertisement. Version messages send the
  null/unroutable addr form (pre-BIP155 tor convention).
- **Policy**: identity-keyed policy (bans, per-address throttles) is
  inapplicable for pathed peers — **fail open**; the bind is the explicit
  grant and filesystem permissions are the access control. Capacity policies
  (connection counts, buffer maxima) apply unchanged.
- **Identity**: "path or nothing" representation where authority (IP:port) is
  assumed — logging, channel identification, peer bookkeeping. Accepted unix
  peers are unnamed (no remote endpoint).

## 7. Roadmap context (capability survey vs Core/btcd)

Correct layering of the remaining gaps:

- **Application layer (peer-protocol tier)**: BIP155/addrv2 —
  `address_item` is v1 (fixed 16-byte IP + port); `send_address_v2` message
  type exists but no variable network-id encoding, hosts storage, or
  in/out protocol handling. This is item zero: it completes Tor
  (socks connector already dials fqdn/onion targets; gossip/storage/
  advertisement is what's missing) and makes CJDNS nearly free (an address
  family + fc00::/8 reachability policy; the transport is plain IPv6 —
  overlay entry is below the socket API via the cjdns TUN).
- **bn network layer**: I2P SAM v3 is the only true transport gap.
  Socks-sibling outbound (line dialect: HELLO/SESSION CREATE/STREAM CONNECT)
  plus three deltas: persistent control-connection session; inbound via
  STREAM ACCEPT (the daemon is the listener — an acceptor analog with no
  existing pattern); node identity is a persisted destination keypair feeding
  addrv2. Per-network timeout settings needed (i2p latency).
- **bs provisioning tier**: tor control client (ADD_ONION, cookie auth) —
  optional by philosophy; a configured onion advertises fine without it once
  addrv2 exists. PCP/NAT-PMP — Core reimplemented natively and now defaults
  on; for us: bs component, one asio UDP socket + renewal timer,
  **operator-configured gateway address** (empty = disabled) eliminating the
  platform routing-table code (the only ugly part; boost provides nothing
  there — the connected-UDP `local_endpoint()` trick portably yields the
  local source address, not the gateway). Last or never.

Stratum: sv1 is a bs service over the existing rpc surface (line-delimited
JSON-RPC, electrum-shaped; `socket.hpp` already annotates
`RPC (TCP: electrum/stratum_v1, WS: btcd)`). sv2 splits as: bn stream upgrade
(6-byte header framing + Noise handshake — secp256k1 and ChaCha20-Poly1305
already in-house, unlike CURVE) + bs TDP (Template Provider) service. Context:
Core exposes only a capnp mining IPC (`-ipcbind`, unix-only, unauthenticated,
permissions-gated) consumed by external sv2-tp and SRI's bridge crate; that
IPC has become the de facto public sv2 interface (node-coupled, already
through a schema-breaking fix cycle for a node-side template leak). A native
TDP endpoint serves every sv2 client at the protocol boundary. No reason for
capnp in libbitcoin: component boundaries here are protocol boundaries.

## 8. Source references (verified this session)

bn (evoskuil/libbitcoin-network master):
- `include/bitcoin/network/net/socket.hpp` — context variant TODO (line 48),
  variant/upgrade architecture, rpc surface annotation.
- `src/net/socket_connect.cpp` — `do_handshake` upgrade branching, v1/v2
  detection, proxied deferral.
- `src/net/socket_ws.cpp` — variant emplacement upgrade pattern.
- `include/bitcoin/network/privacy/stream.hpp` — the stream-type template for
  `zmq::stream` (ownership, async_handshake, message read/write surface).
- `src/net/connector_socks.cpp` — socks5 client; SAM sibling; unix dial site.
- `include/bitcoin/network/messages/peer/detail/address_item.hpp` — v1-only
  address encoding (addrv2 gap).

Core (bitcoin/bitcoin master):
- `src/zmq/zmqpublishnotifier.cpp`, `zmqabstractnotifier.h`,
  `zmqnotificationinterface.cpp`, `doc/zmq.md` — full compat surface (§2).
- `src/interfaces/mining.h`, `-ipcbind` (init.cpp), `doc/multiprocess.md` —
  capnp mining IPC.
- `src/common/pcp.*`, `src/mapport.*` (DEFAULT_NATPMP = true) — native
  PCP/NAT-PMP.
- httpserver/torcontrol now libevent-free (native reimplementation / Sock).

libzmq (zeromq/libzmq master):
- `src/zmtp_engine.cpp` — staged greeting, version selection, v3.0/v3.1
  handshake split, PING/PONG processing, command flagging.
- `src/mechanism.cpp` — Socket-Type compatibility table, READY basic
  properties.
- `src/null_mechanism.cpp` — READY/ERROR command forms.
- `src/v3_1_encoder.cpp` — subscribe/cancel → command conversion.
- `src/dist.cpp` — per-pipe drop-with-rollback semantics.
- `src/curve_*` — libsodium crypto_box (CURVE phase reference).

External:
- gozmq: github.com/lightninglabs/gozmq (`zmq.go`) — lnd's ZMTP 3.0 client.
- Core PR #27679 (zmq unix endpoints, v28.0), #27375 (unix -proxy/-onion),
  #34568 (IPC schema break), issue #33940 (IPC template leak).
