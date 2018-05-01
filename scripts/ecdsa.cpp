#include <iostream>
#include <tuple>
#include <vector>
#include "shared.hpp"

// keys and messages from bitcoinjs-lib/ecdsa test fixtures
// https://github.com/bitcoinjs/bitcoinjs-lib/blob/6b3c41a06c6e38ec79dc2f3389fa2362559b4a46/test/fixtures/ecdsa.json
const auto fkeys = std::vector<std::string>({
	"0000000000000000000000000000000000000000000000000000000000000001",
	"fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364140",
	"fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364140",
	"0000000000000000000000000000000000000000000000000000000000000001",
	"69ec59eaa1f4f2e36b639716b7c30ca86d9a5375c7b38d8918bd9c0ebc80ba64",
	"00000000000000000000000000007246174ab1e92e9149c6e446fe194d072637",
	"000000000000000000000000000000000000000000056916d0f9b31dc9b637f3",
});

const auto messages = std::vector<std::string>({
	"Everything should be made as simple as possible, but not simpler.",
	"Equations are more important to me, because politics is for the present, but an equation is something for eternity.",
	"Not only is the Universe stranger than we think, it is stranger than we can think.",
	"How wonderful that we have met with a paradox. Now we have some hope of making progress.",
	"Computer science is no more about computers than astronomy is about telescopes.",
	"...if you aren't, at any given time, scandalized by code you wrote five or even three years ago, you're not learning anywhere near enough",
	"The question of whether computers can think is like the question of whether submarines can swim.",
});

struct S { uint8_t_32 d; uint8_t_32 m; uint8_t_64 e; std::string desc; bool v = true; };
auto generateSigns () {
	bool ok = true;
	std::vector<S> s;

	size_t i = 0;
	for (const auto& message : messages) {
		const auto fkey = scalarFromHex(fkeys[i++]);
		const auto hash = sha256(message);
		const auto sig = _eccSign(fkey, hash, ok);
		s.push_back({ fkey, hash, sig, message });
	}

	for (const auto& message : messages) {
		const auto rkey = randomPrivate();
		const auto hash = sha256(message);
		const auto sig = _eccSign(rkey, hash, ok);
		s.push_back({ rkey, hash, sig, message });
	}

	s.push_back({ ONE, ZERO, _eccSign(ONE, ZERO, ok), "Strange hash" });
	s.push_back({ ONE, UINT256_MAX, _eccSign(ONE, UINT256_MAX, ok), "Strange hash" });
	s.push_back({ GROUP_ORDER_LESS_1, ZERO, _eccSign(GROUP_ORDER_LESS_1, ZERO, ok), "Stange hash" });
	s.push_back({ GROUP_ORDER_LESS_1, UINT256_MAX, _eccSign(GROUP_ORDER_LESS_1, UINT256_MAX, ok), "Strange hash" });

	// fuzz
	for (int i = 0; i < 10000; i++) {
		const auto rkey = randomPrivate();
		const auto hash = randomScalar();
		auto sig = _eccSign(rkey, hash, ok);
		const auto P = _pointFromScalar<uint8_t_33>(rkey, ok);
		assert(ok);
		auto verified = ok;
		assert(_eccVerify(P, hash, sig) == verified);

		// flip a bit (aka, invalidate the signature)
		if (randomUInt8() > 0x7f) {
			const auto mask = 1 << (1 + (randomUInt8() % 6));
			sig.at(randomUInt8() % 32) ^= mask;
			assert(_eccVerify(P, hash, sig) == false);
			verified = false;
		}

		s.push_back({ rkey, hash, sig, "", verified });
	}

	return s;
}

struct BS { uint8_t_32 d; uint8_t_32 m; std::string except; std::string desc = ""; };
auto generateBadSigns () {
	std::vector<BS> bs;
	for (auto x : generateBadPrivates()) bs.push_back({ x.d, ONE, THROW_BAD_PRIVATE, x.desc });
	return bs;
}

template <typename A> struct BV { A Q; uint8_t_32 m; uint8_t_64 s; std::string except; std::string desc = ""; };
template <typename A>
auto generateBadVerify () {
	bool ok = true;
	const auto G_ONE = _pointFromUInt32<A>(1, ok);
	assert(ok);

	std::vector<BV<A>> bv;
	for (auto x : generateBadPoints<A>()) bv.push_back({ x.P, THREE, _signatureFromRS(ONE, ONE), THROW_BAD_POINT, x.desc });
	for (auto x : generateBadSignatures()) bv.push_back({ G_ONE, THREE, x.P, THROW_BAD_SIGNATURE, x.desc });
	return bv;
}

template <typename T>
void dumpJSON (std::ostream& o, const T& t) {
	o << jsonifyO({
		jsonp("valid", jsonifyA(std::get<0>(t), [&](auto x) {
			return jsonifyO({
				x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
				jsonp("d", jsonify(x.d)),
				jsonp("m", jsonify(x.m)),
				jsonp("signature", jsonify(x.e)),
				jsonp("verifies", jsonify(x.v))
			});
		})),
		jsonp("invalid", jsonifyO({
			jsonp("sign", jsonifyA(std::get<1>(t), [&](auto x) {
				return jsonifyO({
					x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
					jsonp("exception", jsonify(x.except)),
					jsonp("d", jsonify(x.d)),
					jsonp("m", jsonify(x.m))
				});
			})),
			jsonp("verify", jsonifyA(std::get<2>(t), [&](auto x) {
				return jsonifyO({
					x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
					jsonp("exception", jsonify(x.except)),
					jsonp("Q", jsonify(x.Q)),
					jsonp("m", jsonify(x.m)),
					jsonp("signature", jsonify(x.s))
				});
			}))
		}))
	});
}

int main () {
	_ec_init();
	const auto s = generateSigns();
	const auto bs = generateBadSigns();
	const auto bv = generateBadVerify<uint8_t_33>();

	dumpJSON(std::cout, std::make_tuple(s, bs, bv));

	return 0;
}
