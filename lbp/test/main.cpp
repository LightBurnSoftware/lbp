#include <cassert>
#include <iostream>

#include <lbp/message.h>
#include <lbp/parser.h>
#include <lbp/payload.h>
#include <lbp/queue.h>
#include <lbp/spec.h>

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

	std::cout << "lbp test suite: success" << std::endl;

	return 0;
}
