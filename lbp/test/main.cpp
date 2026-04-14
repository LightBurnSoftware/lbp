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
	lbp::Parser<128> parser;

	lbp::CmdMsg msg(lbp::cmd_move_abs_xy, 8);

	msg.writeInt(333);
	msg.writeInt(444);

	parser.feed(msg.data(), msg.size());

	// If parsing is successful, that means that the msg formed a proper checksum.
	assert(parser.parseNext() == true);

	lbp::MaxPayload &mp = parser.payload();

	assert(mp.size() == 10);
	assert(mp.cmd() == lbp::cmd_move_abs_xy);
	assert(mp.readIntArg() == 333);
	assert(mp.readIntArg() == 444);

	lbp::Queue<lbp::CmdPayload, 32> queue;

	// Test Queue and Payload copying
	queue.push(toCmd(mp));
	lbp::CmdPayload cp = queue.pop();

	assert(cp.size() == 10);
	assert(cp.cmd() == lbp::cmd_move_abs_xy);
	assert(cp.readIntArg() == 333);
	assert(cp.readIntArg() == 444);

	std::cout << "lbp test suite: success" << std::endl;

	return 0;
}
