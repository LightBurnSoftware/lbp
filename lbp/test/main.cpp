#include <cassert>
#include <cstring>
#include <iostream>

#include <lbp/message.h>
#include <lbp/parser.h>
#include <lbp/payload.h>
#include <lbp/queue.h>
#include <lbp/spec.h>
#include <lbp/version.h>

int main()
{
	std::cout << "lbp test suite: start" << std::endl;
	lbp::Parser<512> parser;

	lbp::CmdMsg msg(lbp::cmd_jog_to_xy, 8);

	assert(msg.writeInt(333));
	assert(msg.writeInt(444));

	assert(parser.feed(msg.data(), msg.size()));

	// If parsing is successful, that means that the msg formed a proper checksum.
	assert(parser.parseNext() == true);

	lbp::MaxPayload &mp = parser.payload();

	assert(mp.size() == 10);
	assert(mp.cmd() == lbp::cmd_jog_to_xy);
	int32_t x, y = 0;
	assert(mp.readArg(x));
	assert(x == 333);
	assert(mp.readArg(y));
	assert(y == 444);

	lbp::Queue<lbp::CmdPayload, 32> queue;

	// Test Queue and Payload copying
	assert(queue.push(toCmd(mp)));
	lbp::CmdPayload cp;
	assert(queue.pop(cp));

	assert(cp.size() == 10);
	assert(cp.cmd() == lbp::cmd_jog_to_xy);

	x = y = 0;
	if (cp.readArgs(x, y)) {
		assert(x == 333);
		assert(y == 444);
	}

	lbp::CmdPayload moved = std::move(cp);

	// Wire format matches the spec.md handshake example: [DRGN][txid][size][cmd][crc]
	{
		lbp::CmdMsg hs(lbp::cmd_handshake);
		assert(hs.complete());
		assert(hs.txid() == lbp::txid_none);
		assert(hs.setTxid(1));
		const uint8_t expected[] = {0x44, 0x52, 0x47, 0x4e, 0x00, 0x01, 0x00, 0x02, 0x01, 0xb8, 0x68, 0x69};
		assert(hs.size() == sizeof(expected));
		assert(std::memcmp(hs.data(), expected, sizeof(expected)) == 0);
	}

	// Position query response example, including txid round trip through the parser and toCmd.
	{
		lbp::CmdMsg req(lbp::cmd_pos_x);
		assert(req.setTxid(0x2a));
		assert(parser.feed(req.data(), req.size()) == req.size());
		assert(parser.parseNext());
		assert(parser.payload().txid() == 0x2a);

		auto resp = lbp::CmdMsg::responseTo(parser.payload(), 4);
		assert(!resp.complete());
		assert(resp.writeInt(20000));
		assert(resp.complete());
		assert(resp.txid() == 0x2a);
		assert(resp.cmd() == lbp::cmd_pos_x);
		const uint8_t expected[] = {0x44, 0x52, 0x47, 0x4e, 0x00, 0x2a, 0x00, 0x06,
									0x81, 0x01, 0x00, 0x00, 0x4e, 0x20, 0x05, 0xdf};
		assert(resp.size() == sizeof(expected));
		assert(std::memcmp(resp.data(), expected, sizeof(expected)) == 0);

		assert(parser.feed(resp.data(), resp.size()) == resp.size());
		assert(parser.parseNext());
		lbp::CmdPayload c = toCmd(parser.payload());
		assert(c.txid() == 0x2a);
		int32_t pos = 0;
		assert(c.readArg(pos) && pos == 20000);
	}

	// The checksum covers the txid: a corrupted txid is rejected.
	{
		lbp::CmdMsg m(lbp::cmd_handshake);
		assert(m.setTxid(7));
		uint8_t raw[lbp::size_min_msg];
		std::memcpy(raw, m.data(), m.size());
		raw[lbp::size_txid_offset + 1] ^= 0x01;
		const size_t rejected = parser.checksum_rejections;
		assert(parser.feed(raw, sizeof(raw)) == sizeof(raw));
		assert(!parser.parseNext());
		assert(parser.checksum_rejections == rejected + 1);

		// ...and the parser recovers for the next good message.
		assert(parser.feed(m.data(), m.size()) == m.size());
		assert(parser.parseNext());
		assert(parser.payload().txid() == 7);
	}

	// Incremental crc matches one-shot crc.
	{
		const uint8_t a[] = {1, 2, 3, 4, 5, 6, 7};
		uint16_t crc = lbp::crc16_begin();
		crc = lbp::crc16_update(crc, a, 3);
		crc = lbp::crc16_update(crc, a + 3, 4);
		assert(lbp::crc16_end(crc) == lbp::crc16(a, sizeof(a)));
		assert(lbp::crc16(a, 0) == lbp::crc16_end(lbp::crc16_begin()));
	}

	// Version compatibility
	static_assert(lbp::versionsCompatible(lbp::makeVersion(0, 9), lbp::makeVersion(0, 9)));
	static_assert(!lbp::versionsCompatible(lbp::makeVersion(0, 9), lbp::makeVersion(0, 8)));
	static_assert(lbp::versionsCompatible(lbp::makeVersion(1, 2), lbp::makeVersion(1, 0)));
	static_assert(!lbp::versionsCompatible(lbp::makeVersion(2, 0), lbp::makeVersion(1, 9)));

	std::cout << "lbp test suite: success" << std::endl;

	return 0;
}
