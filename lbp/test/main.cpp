#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <vector>

#include <lbp/lbpcor.h>
#include <lbp/message.h>
#include <lbp/parser.h>
#include <lbp/payload.h>
#include <lbp/queue.h>
#include <lbp/spec.h>
#include <lbp/version.h>

namespace {

// A cell's (dx, dy) in nm, given its column and row.
using CellFn = std::function<void(int col, int row, int32_t &dx, int32_t &dy)>;

std::vector<uint8_t> makeLbpCor(int32_t field_um, uint16_t grid, const CellFn &cell, uint16_t header_size = 24)
{
	std::vector<uint8_t> f(header_size + size_t(grid) * grid * 8, 0);
	std::memcpy(f.data(), lbp::lbpcor_magic, sizeof(lbp::lbpcor_magic));
	lbp::writeBE16(lbp::lbpcor_version, &f[lbp::lbpcor_offset_version]);
	lbp::writeBE16(header_size, &f[lbp::lbpcor_offset_header_size]);
	lbp::writeBE32(uint32_t(field_um), &f[lbp::lbpcor_offset_field_size]);
	lbp::writeBE16(grid, &f[lbp::lbpcor_offset_grid_size]);
	f[lbp::lbpcor_offset_axis_flags] = lbp::lbpcor_axis_swap;
	f[lbp::lbpcor_offset_composition] = lbp::lbpcor_has_scale | lbp::lbpcor_has_residual;
	f[lbp::lbpcor_offset_cell_format] = lbp::lbpcor_cell_int32_nm;
	for (int row = 0; row < grid; row++) {
		for (int col = 0; col < grid; col++) {
			int32_t dx = 0, dy = 0;
			cell(col, row, dx, dy);
			uint8_t *c = &f[header_size + (size_t(row) * grid + col) * 8];
			lbp::writeBE32(uint32_t(dx), c);
			lbp::writeBE32(uint32_t(dy), c + 4);
		}
	}
	return f;
}

// The spec's interpolation, written plainly in double: the reference the fixed-point version is held to.
void referenceDistortion(const lbp::CorrectionTable &t, double x_um, double y_um, double &dx, double &dy)
{
	const int n = t.gridSize();
	auto locate = [&](double pos, int &cell, double &frac) {
		double u = (pos + t.fieldSize() / 2.0) * (n - 1) / t.fieldSize();
		u = std::fmin(std::fmax(u, 0.0), double(n - 1));
		cell = std::min(int(std::floor(u)), n - 2);
		frac = u - cell;
	};
	int col, row;
	double fx, fy;
	locate(x_um, col, fx);
	locate(y_um, row, fy);
	int32_t x00, y00, x10, y10, x01, y01, x11, y11;
	t.node(col, row, x00, y00);
	t.node(col + 1, row, x10, y10);
	t.node(col, row + 1, x01, y01);
	t.node(col + 1, row + 1, x11, y11);
	dx = (x00 * (1 - fx) + x10 * fx) * (1 - fy) + (x01 * (1 - fx) + x11 * fx) * fy;
	dy = (y00 * (1 - fx) + y10 * fx) * (1 - fy) + (y01 * (1 - fx) + y11 * fx) * fy;
}

double largestStep(const lbp::CorrectionTable &t);

// The documented accuracy: 1 nm of rounding, plus the fraction's resolution (1/2^20 of a cell, along each axis)
// times the largest difference between neighbouring nodes. Past 2^25 um, rounding the scale up adds up to
// 2 * field / 2^26 more of those units.
double tolerance(const lbp::CorrectionTable &t)
{
	const double units = 1.0 + 2.0 * t.fieldSize() / double(1 << 26);
	return 1.0 + 2.0 * units * largestStep(t) / double(1 << 20) + 1e-6;
}

// The largest difference between neighbouring nodes, in nm: the fraction's resolution error scales with it.
double largestStep(const lbp::CorrectionTable &t)
{
	double worst = 0;
	for (int row = 0; row < t.gridSize(); row++) {
		for (int col = 0; col < t.gridSize(); col++) {
			int32_t dx, dy, nx, ny;
			t.node(col, row, dx, dy);
			if (col + 1 < t.gridSize()) {
				t.node(col + 1, row, nx, ny);
				worst = std::fmax(worst, std::fmax(std::fabs(double(nx) - dx), std::fabs(double(ny) - dy)));
			}
			if (row + 1 < t.gridSize()) {
				t.node(col, row + 1, nx, ny);
				worst = std::fmax(worst, std::fmax(std::fabs(double(nx) - dx), std::fabs(double(ny) - dy)));
			}
		}
	}
	return worst;
}

uint32_t nextRandom(uint32_t &state)
{
	state = state * 1664525u + 1013904223u;
	return state;
}

// Random positions across (and a little beyond) the field, against the double reference.
void checkAgainstReference(const lbp::CorrectionTable &t, uint32_t seed, int count = 20000)
{
	const double limit = tolerance(t);
	const int64_t span = int64_t(t.fieldSize()) * 3 / 2; // 25% past each edge
	for (int i = 0; i < count; i++) {
		const int32_t x = int32_t(int64_t(nextRandom(seed) % uint32_t(span)) - span / 2);
		const int32_t y = int32_t(int64_t(nextRandom(seed) % uint32_t(span)) - span / 2);
		int32_t dx, dy;
		double rx, ry;
		t.distortion(x, y, dx, dy);
		referenceDistortion(t, x, y, rx, ry);
		if (std::fabs(dx - rx) > limit || std::fabs(dy - ry) > limit) {
			std::cout << "lbpcor: field " << t.fieldSize() << " grid " << t.gridSize() << " at (" << x << ", " << y
					  << "): (" << dx << ", " << dy << ") vs (" << rx << ", " << ry << "), tolerance " << limit
					  << std::endl;
			assert(false);
		}
	}
}

void testCorrectionTable()
{
	using lbp::CorrectionTable;
	uint32_t detail = 0;

	// A realistic table: pincushion growing to about 2 mm at the corners, plus up to +-20 um of measured noise.
	auto pincushion = [](int32_t field, uint16_t grid) {
		return [field, grid](int col, int row, int32_t &dx, int32_t &dy) {
			const double c = (grid - 1) / 2.0;
			const double x = (col - c) / c, y = (row - c) / c; // -1 .. 1
			const double k = 1e6 * field / 100000.0;		  // nm at the corners, for a 100 mm field
			uint32_t s = uint32_t(row * 7919 + col * 104729);
			dx = int32_t(k * x * (y * y) + double(nextRandom(s) % 40001) - 20000);
			dy = int32_t(k * y * (x * x) + double(nextRandom(s) % 40001) - 20000);
		};
	};

	// Receipt checks, each reporting the field it found wrong.
	{
		const std::vector<uint8_t> good = makeLbpCor(100000, 65, pincushion(100000, 65));
		CorrectionTable t;
		assert(t.parse(good.data(), good.size(), detail) == 0 && detail == 0 && t.valid());
		assert(t.fieldSize() == 100000 && t.gridSize() == 65);
		assert(t.axisFlags() == lbp::lbpcor_axis_swap);
		assert(t.composition() == (lbp::lbpcor_has_scale | lbp::lbpcor_has_residual));
		assert(good.size() == 33824);

		auto refused = [&](std::vector<uint8_t> f, uint16_t error, uint32_t offset, uint16_t max_grid = 0xFFFF) {
			CorrectionTable bad;
			uint32_t d = 0xDEAD;
			const bool ok = bad.parse(f.data(), f.size(), d, max_grid) == error && d == offset && !bad.valid();
			return ok;
		};
		auto with = [&](size_t at, uint8_t value) {
			std::vector<uint8_t> f = good;
			f[at] = value;
			return f;
		};

		assert(refused(with(lbp::lbpcor_offset_magic + 3, 'X'), lbp::err_invalid_header, lbp::lbpcor_offset_magic));
		assert(refused(with(lbp::lbpcor_offset_version + 1, 2), lbp::err_invalid_header, lbp::lbpcor_offset_version));
		assert(refused(with(lbp::lbpcor_offset_header_size + 1, 20), lbp::err_invalid_header,
					   lbp::lbpcor_offset_header_size));
		assert(refused(with(lbp::lbpcor_offset_field_size, 0x80), lbp::err_invalid_header,
					   lbp::lbpcor_offset_field_size)); // negative
		assert(refused(with(lbp::lbpcor_offset_grid_size + 1, 64), lbp::err_invalid_header,
					   lbp::lbpcor_offset_grid_size)); // even
		assert(refused(with(lbp::lbpcor_offset_grid_size + 1, 1), lbp::err_invalid_header,
					   lbp::lbpcor_offset_grid_size)); // too small
		assert(refused(good, lbp::err_invalid_header, lbp::lbpcor_offset_grid_size, 33)); // over this firmware's limit
		assert(refused(with(lbp::lbpcor_offset_cell_format, 2), lbp::err_invalid_header,
					   lbp::lbpcor_offset_cell_format));

		std::vector<uint8_t> shorter = good;
		shorter.pop_back();
		assert(refused(shorter, lbp::err_invalid_file, 0));
		assert(refused(std::vector<uint8_t>(good.begin(), good.begin() + 10), lbp::err_invalid_file, 0));

		// Ignored: LightBurn's flags and the reserved bytes.
		std::vector<uint8_t> f = with(21, 0xFF);
		f[23] = 0x55;
		f[lbp::lbpcor_offset_axis_flags] = 0xFF;
		f[lbp::lbpcor_offset_composition] = 0xFF;
		CorrectionTable t2;
		assert(t2.parse(f.data(), f.size(), detail) == 0);

		// A longer header from a later minor revision: skipped using header_size.
		const std::vector<uint8_t> longer = makeLbpCor(100000, 65, pincushion(100000, 65), 32);
		assert(t2.parse(longer.data(), longer.size(), detail) == 0);
		int32_t a, b, c, e;
		t.node(10, 20, a, b);
		t2.node(10, 20, c, e);
		assert(a == c && b == e);

		// A cell larger than half the field (here, a table written in micrometers for a 1 mm field, but with one
		// cell scaled as nm): refused only when the firmware makes the optional check.
		const std::vector<uint8_t> wild = makeLbpCor(1000, 3, [](int col, int row, int32_t &dx, int32_t &dy) {
			dx = (col == 1 && row == 1) ? 500001 : 0;
			dy = 0;
		});
		assert(t2.parse(wild.data(), wild.size(), detail) == lbp::err_invalid_file && !t2.valid());
		assert(t2.parse(wild.data(), wild.size(), detail, 0xFFFF, false) == 0);
	}

	// No table: no correction.
	{
		CorrectionTable t;
		int32_t dx = 1, dy = 1;
		t.distortion(1234, -5678, dx, dy);
		assert(dx == 0 && dy == 0);
		int64_t x, y;
		t.correct(1234, -5678, x, y);
		assert(x == 1234000 && y == -5678000);
	}

	// Exact at every node, including fields near the 2^25 um limit of that guarantee.
	for (int32_t field : {96000, 64 * 524287}) {
		const uint16_t grid = 65;
		const std::vector<uint8_t> f = makeLbpCor(field, grid, pincushion(field, grid));
		CorrectionTable t;
		assert(t.parse(f.data(), f.size(), detail, 0xFFFF, false) == 0);
		const int32_t step = field / (grid - 1);
		for (int row = 0; row < grid; row++) {
			for (int col = 0; col < grid; col++) {
				int32_t dx, dy, nx, ny;
				t.distortion((col - 32) * step, (row - 32) * step, dx, dy);
				t.node(col, row, nx, ny);
				assert(dx == nx && dy == ny);
			}
		}
	}

	// Between nodes, against the double reference: even and odd fields, coarse and fine grids, the smallest grid.
	{
		struct Case {
			int32_t field;
			uint16_t grid;
		};
		uint32_t seed = 1;
		for (Case c : {Case{100000, 65}, Case{78821, 65}, Case{300000, 33}, Case{110001, 3}, Case{1000, 255}}) {
			const std::vector<uint8_t> f = makeLbpCor(c.field, c.grid, pincushion(c.field, c.grid));
			CorrectionTable t;
			assert(t.parse(f.data(), f.size(), detail, 0xFFFF, false) == 0);
			checkAgainstReference(t, seed++);
		}
	}

	// Extremes: the largest field, cells at both int32 limits, positions at both int32 limits. Nothing overflows.
	{
		const int32_t field = 0x7FFFFFFF;
		const std::vector<uint8_t> f = makeLbpCor(field, 3, [](int col, int row, int32_t &dx, int32_t &dy) {
			dx = ((col + row) % 2) ? INT32_MAX : INT32_MIN;
			dy = ((col + row) % 2) ? INT32_MIN : INT32_MAX;
		});
		CorrectionTable t;
		assert(t.parse(f.data(), f.size(), detail, 0xFFFF, false) == 0);
		checkAgainstReference(t, 99);
		for (int32_t x : {INT32_MIN, -1, 0, 1, INT32_MAX}) {
			for (int32_t y : {INT32_MIN, -1, 0, 1, INT32_MAX}) {
				int32_t dx, dy;
				double rx, ry;
				t.distortion(x, y, dx, dy);
				referenceDistortion(t, x, y, rx, ry);
				assert(std::fabs(dx - rx) <= tolerance(t));
				assert(std::fabs(dy - ry) <= tolerance(t));
			}
		}
	}

	// Outside the field: the distortion at the nearest point on the edge. correct() subtracts it from the
	// unclamped position.
	{
		const std::vector<uint8_t> f = makeLbpCor(100000, 65, pincushion(100000, 65));
		CorrectionTable t;
		assert(t.parse(f.data(), f.size(), detail) == 0);
		int32_t ex, ey, ox, oy;
		t.distortion(50000, -12345, ex, ey);
		t.distortion(80000, -12345, ox, oy);
		assert(ex == ox && ey == oy);
		t.distortion(-50000, -50000, ex, ey);
		t.distortion(-90000, -70000, ox, oy);
		assert(ex == ox && ey == oy);

		int64_t x, y;
		t.correct(80000, -12345, x, y);
		t.distortion(80000, -12345, ox, oy);
		assert(x == 80000000 - ox && y == -12345000 - oy);
	}
}

} // namespace

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

	assert(mp.size() == 8);
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

	assert(cp.size() == 8);
	assert(cp.cmd() == lbp::cmd_jog_to_xy);

	x = y = 0;
	if (cp.readArgs(x, y)) {
		assert(x == 333);
		assert(y == 444);
	}

	lbp::CmdPayload moved = std::move(cp);

	// Wire format: [DRGN][txid][cmd][size][args][crc]
	{
		lbp::CmdMsg hs(lbp::cmd_handshake);
		assert(hs.complete());
		assert(hs.txid() == lbp::txid_none);
		assert(hs.setTxid(1));
		const uint8_t expected[] = {0x44, 0x52, 0x47, 0x4e, 0x00, 0x01, 0x01, 0xb8, 0x00, 0x00, 0xac, 0xf0};
		assert(hs.size() == sizeof(expected));
		assert(std::memcmp(hs.data(), expected, sizeof(expected)) == 0);
	}

	// Checksum is CRC-16/X-25 (check value 0x906E), sent big-endian: see spec.md "Checksum".
	{
		const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
		assert(lbp::crc16(check, sizeof(check)) == 0x906E);
		uint8_t wire[2];
		lbp::writeBE16(lbp::crc16(check, sizeof(check)), wire);
		assert(wire[0] == 0x90 && wire[1] == 0x6e);
	}

	// File transfers end with a CRC-32 (zlib, cmd_file_end): check value 0xCBF43926.
	{
		const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
		assert(lbp::crc32(check, sizeof(check)) == 0xCBF43926u);
		uint32_t crc = lbp::crc32_begin();
		crc = lbp::crc32_update(crc, check, 4);
		crc = lbp::crc32_update(crc, check + 4, sizeof(check) - 4);
		assert(lbp::crc32_end(crc) == 0xCBF43926u);
		assert(lbp::crc32(check, 0) == lbp::crc32_end(lbp::crc32_begin()));
	}

	// The handshake example in spec.md.
	{
		lbp::CmdMsg hs(lbp::cmd_handshake, 2, lbp::protocol_version);
		assert(hs.setTxid(1));
		const uint8_t expected[] = {0x44, 0x52, 0x47, 0x4e, 0x00, 0x01, 0x01, 0xb8, 0x00, 0x02, 0x00, 0x0a, 0x06, 0xcb};
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
		const uint8_t expected[] = {0x44, 0x52, 0x47, 0x4e, 0x00, 0x2a, 0x81, 0x01,
									0x00, 0x04, 0x00, 0x00, 0x4e, 0x20, 0x75, 0x9d};
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

	// nackTo: cmd_nack carrying the request's txid, rejected cmd, code, and detail.
	{
		lbp::CmdMsg req(lbp::cmd_jog_to_xy, 8);
		assert(req.writeInt(1) && req.writeInt(2));
		assert(req.setTxid(0x1234));
		assert(parser.feed(req.data(), req.size()) == req.size());
		assert(parser.parseNext());

		auto nack = lbp::CmdMsg::nackTo(parser.payload(), lbp::err_invalid_arg, 1);
		assert(nack.complete());
		assert(nack.cmd() == lbp::cmd_nack);
		assert(nack.txid() == 0x1234);

		assert(parser.feed(nack.data(), nack.size()) == nack.size());
		assert(parser.parseNext());
		lbp::MaxPayload &p = parser.payload();
		assert(p.txid() == 0x1234);
		uint16_t rejected = 0, code = 0;
		uint32_t detail = 0;
		assert(p.readArgs(rejected, code, detail));
		assert(rejected == lbp::cmd_jog_to_xy);
		assert(code == lbp::err_invalid_arg);
		assert(detail == 1);
	}

	// Zero-argument messages round trip with size 0.
	{
		lbp::CmdMsg m(lbp::cmd_get_state);
		assert(m.size() == lbp::size_min_msg && m.argSize() == 0);
		assert(parser.feed(m.data(), m.size()) == m.size());
		assert(parser.parseNext());
		assert(parser.payload().cmd() == lbp::cmd_get_state);
		assert(parser.payload().size() == 0);
	}

	// The largest argument size is accepted; one byte more is rejected as a bad size.
	{
		static uint8_t chunk[lbp::size_file_args] = {};
		lbp::MaxMsg big(lbp::cmd_file_chunk, lbp::size_file_args);
		assert(big.writeData(chunk, lbp::size_file_args));
		assert(big.complete() && big.size() == lbp::size_file_msg);
		assert(parser.feed(big.data(), big.size()) == big.size());
		assert(parser.parseNext());
		assert(parser.payload().size() == lbp::size_file_args);

		uint8_t hdr[lbp::size_header] = {0x44, 0x52, 0x47, 0x4e, 0x00, 0x00, 0x44, 0xFC, 0x00, 0x00};
		lbp::writeBE16(lbp::size_file_args + 1, hdr + lbp::size_len_offset);
		const size_t rejected = parser.size_rejections;
		assert(parser.feed(hdr, sizeof(hdr)) == sizeof(hdr));
		assert(!parser.parseNext());
		assert(parser.size_rejections == rejected + 1);
	}

	// Message offsets: each parsed message's first byte, counted since clear(), even past a corrupted message.
	{
		lbp::Parser<512> p;
		const lbp::CmdMsg m1(lbp::cmd_dwell, 4, 100);			// 16 bytes
		const lbp::CmdMsg m2(lbp::cmd_laser_on, 1, 0);			// 13 bytes
		const lbp::CmdMsg m3(lbp::cmd_dwell, 4, 200);			// 16 bytes
		uint8_t bad[lbp::size_file_msg];
		std::memcpy(bad, m2.data(), m2.size());
		bad[m2.size() - 1] ^= 0xFF;								// corrupt the checksum

		assert(p.feed(m1.data(), m1.size()) == m1.size());
		assert(p.feed(m2.data(), m2.size()) == m2.size());
		assert(p.feed(bad, m2.size()) == m2.size());
		assert(p.feed(m3.data(), m3.size()) == m3.size());

		assert(p.parseNext() && p.payload().cmd() == lbp::cmd_dwell && p.messageOffset() == 0);
		assert(p.parseNext() && p.payload().cmd() == lbp::cmd_laser_on && p.messageOffset() == m1.size());
		// The corrupted copy is skipped; m3 still reports its true position.
		assert(p.parseNext() && p.payload().cmd() == lbp::cmd_dwell);
		assert(p.messageOffset() == size_t(m1.size() + m2.size() + m2.size()));
		assert(p.checksum_rejections == 1);
		assert(!p.parseNext());

		p.clear();
		assert(p.messageOffset() == 0);
	}

	testCorrectionTable();

	// Version compatibility
	static_assert(lbp::versionsCompatible(lbp::makeVersion(0, 9), lbp::makeVersion(0, 9)));
	static_assert(!lbp::versionsCompatible(lbp::makeVersion(0, 9), lbp::makeVersion(0, 8)));
	static_assert(lbp::versionsCompatible(lbp::makeVersion(1, 2), lbp::makeVersion(1, 0)));
	static_assert(!lbp::versionsCompatible(lbp::makeVersion(2, 0), lbp::makeVersion(1, 9)));

	std::cout << "lbp test suite: success" << std::endl;

	return 0;
}
