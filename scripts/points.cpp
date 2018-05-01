#include <iostream>
#include <tuple>
#include <vector>
#include "shared.hpp"

// ref https://github.com/bitcoin-core/secp256k1/blob/6ad5cdb42a1a8257289a0423d644dcbdeab0f83c/src/tests.c#L2160
//   iteratively verifies that (d + ...)G == (dG + ...G)
template <typename A, typename B, typename C, typename D>
void test_ec_combine (B& pa, C& pas, D& pfs) {
	bool ok = true;
	auto sum = ONE;
	auto sumQ = _pointFromScalar<A>(sum, ok);
	assert(ok);

	for (int i = 1; i <= 10; i++) {
		const auto d = randomPrivate();
		const auto Q = _pointFromScalar<A>(d, ok);
		assert(ok);

		// dG + ...G
		const auto P = _pointAdd<A>(sumQ, Q, ok);
		assert(ok);

		// (d + ...)G
		const auto U = _pointAddScalar<A>(sumQ, d, ok);
		assert(ok);
		assert(P == U);

		// (d + ...)G
		sum = _privAdd(sum, d, ok);
		assert(ok);

		const auto R = _pointFromScalar<A>(sum, ok);
		assert(ok);
		assert(P == R);

		pa.push_back({ sumQ, Q, P });
		pas.push_back({ sumQ, d, P });
		pfs.push_back({ sum, P });

		sumQ = P;
	}
}

struct PC { uint8_t_vec a; bool b; uint8_t_vec e; };
auto generatePC () {
	std::vector<PC> pc;
	pc.push_back({ vectorify(GENERATOR), true, vectorify(GENERATORC) });
	pc.push_back({ vectorify(GENERATOR), false, vectorify(GENERATOR) });
	pc.push_back({ vectorify(GENERATORC), true, vectorify(GENERATORC) });
	pc.push_back({ vectorify(GENERATORC), false, vectorify(GENERATOR) });
	pc.push_back({ uint8_t_vec(33, 0), false, {} });
	pc.push_back({ uint8_t_vec(33, 0), true, {} });
	pc.push_back({ uint8_t_vec(65, 0), false, {} });
	pc.push_back({ uint8_t_vec(65, 0), true, {} });

	bool ok = true;
	for (auto i = 1; i < 10; ++i) {
		const auto iic = vectorify(_pointFromUInt32<uint8_t_33>(i, ok));
		const auto ii = vectorify(_pointFromUInt32<uint8_t_65>(i, ok));
		assert(ok);

		pc.push_back({ iic, true, iic });
		pc.push_back({ iic, false, ii });
		pc.push_back({ ii, true, iic });
		pc.push_back({ ii, false, ii });
	}

	return pc;
}

template <typename A> struct IP { A a; bool e; std::string desc = ""; };
template <typename A> struct PA { A a; A b; A e; std::string except = ""; std::string desc = ""; };
template <typename A> struct PAS { A a; uint8_t_32 b; A e; std::string except = ""; std::string desc = ""; };
template <typename A> struct PFS { uint8_t_32 a; A e; std::string except = ""; std::string desc = ""; };
template <typename A>
auto generate (const A G) {
	bool ok = true;
	const auto G_LESS_1 = _pointFromScalar<A>(GROUP_ORDER_LESS_1, ok);
	const auto G_LESS_2 = _pointFromScalar<A>(GROUP_ORDER_LESS_2, ok);
	const auto G_LESS_3 = _pointFromScalar<A>(GROUP_ORDER_LESS_3, ok);
	const auto G_ONE = _pointFromUInt32<A>(1, ok);
	const auto G_TWO = _pointFromUInt32<A>(2, ok);
	const auto G_THREE = _pointFromUInt32<A>(3, ok);
	auto NULLQ = A();
	NULLQ.fill(0xff);
	assert(ok);

	///////////////////////////////// isPoint
	std::vector<IP<A>> ip;
	ip.push_back({ G, true });
	ip.push_back({ A{0x2}, false });
	ip.push_back({ A{0x3}, false });
	ip.push_back({ A{0x4}, false });
	ip.push_back({ G_ONE, true });
	ip.push_back({ G_TWO, true });
	ip.push_back({ G_THREE, true });

	const auto badPoints = generateBadPoints<A>();
	for (const auto x : badPoints) {
		ip.push_back({ x.P, false, x.desc });
	}

	for (size_t i = 0; i < 100; ++i) {
		ip.push_back({ _pointFromScalar<A>(randomPrivate(), ok), true });
		assert(ok);
	}

	///////////////////////////////// pointAdd
	std::vector<PA<A>> pa;
	pa.push_back({ G_LESS_1, G_LESS_1, G_LESS_2 });
	pa.push_back({ G_LESS_1, G_LESS_2, G_LESS_3 });
	pa.push_back({ G_LESS_1, G_LESS_2, G_LESS_3 });

	// https://github.com/bitcoin-core/secp256k1/blob/452d8e4d2a2f9f1b5be6b02e18f1ba102e5ca0b4/src/tests.c#L3857
	pa.push_back({ G_ONE, G_LESS_1, NULLQ }); // == 0/infinity
	pa.push_back({ G_ONE, G_LESS_2, G_LESS_1 }); // == -1
	pa.push_back({ G_TWO, G_LESS_1, G_ONE }); // == 1
	pa.push_back({ G_ONE, G, NULLQ });
	pa.push_back({ G_ONE, G_ONE, G_TWO });
	pa.push_back({ G_ONE, G_TWO, G_THREE });

	for (size_t i = 0; i < 100; ++i) {
		const auto a = _pointFromScalar<A>(randomPrivate(), ok);
		const auto b = _pointFromScalar<A>(randomPrivate(), ok);
		const auto e = _pointAdd(a, b, ok);
		assert(ok);
		pa.push_back({ a, b, e });
	}

	std::vector<PA<A>> paf;
	for (const auto x : badPoints) {
		paf.push_back({ x.P, G_ONE, {}, THROW_BAD_POINT, x.desc });
		paf.push_back({ G_ONE, x.P, {}, THROW_BAD_POINT, x.desc });
	}

	///////////////////////////////// pointAddScalar
	std::vector<PAS<A>> pas;

	// #L3719, -1 + 0 == -1
	pas.push_back({ G_LESS_1, ZERO, G_LESS_1 });
	pas.push_back({ G_LESS_1, ONE, NULLQ, "", "Adds to infinity" });
	pas.push_back({ G_LESS_1, TWO, G_ONE });
	pas.push_back({ G_LESS_1, THREE, G_TWO });
	pas.push_back({ G_LESS_1, GROUP_ORDER_LESS_1, G_LESS_2 });
	pas.push_back({ G_LESS_1, GROUP_ORDER_LESS_2, G_LESS_3 });
	pas.push_back({ G_LESS_1, GROUP_ORDER_LESS_2, G_LESS_3 });
	pas.push_back({ G_LESS_2, ONE, G_LESS_1 });
	pas.push_back({ G_LESS_2, TWO, NULLQ, "", "Adds to infinity" });
	pas.push_back({ G_LESS_2, THREE, G_ONE });
	pas.push_back({ G_ONE, GROUP_ORDER_LESS_1, NULLQ, "", "Adds to infinity" });
	pas.push_back({ G_ONE, GROUP_ORDER_LESS_2, G_LESS_1, "", "== G - 1" }); // == -1
	pas.push_back({ G_TWO, GROUP_ORDER_LESS_1, G_ONE, "", "== 1" }); // == 1

	std::vector<PAS<A>> pasf;
	const auto badTweaks = generateBadTweaks();
	for (const auto x : badPoints) pasf.push_back({ x.P, ONE, {}, THROW_BAD_POINT, x.desc });
	for (const auto x : badTweaks) pasf.push_back({ G_ONE, x.d, {}, THROW_BAD_TWEAK, x.desc });

	for (uint32_t i = 1; i < 5; ++i) {
		bool ok = true;
		const auto G_i = _pointFromUInt32<A>(i, ok); assert(ok);
		const auto G_i_p1 = _pointFromUInt32<A>(i + 1, ok); assert(ok);

		pas.push_back({ G_i, ONE, G_i_p1 });
	}

	///////////////////////////////// pointFromScalar
	std::vector<PFS<A>> pfs;
	pfs.push_back({ ONE, G_ONE }); // #L3153, #L3692, OK, > 0
	pfs.push_back({ TWO, G_TWO });
	pfs.push_back({ THREE, G_THREE });
	pfs.push_back({ GROUP_ORDER_LESS_1, G_LESS_1 }); // #L3171, #L3710, OK == G - 1
	pfs.push_back({ GROUP_ORDER_LESS_2, G_LESS_2 });
	pfs.push_back({ GROUP_ORDER_LESS_3, G_LESS_3 });

	std::vector<PFS<A>> pfsf;
	const auto badPrivates = generateBadPrivates();
	for (const auto x : badPrivates) pfsf.push_back({ x.d, {}, THROW_BAD_PRIVATE, x.desc });

	// ref https://github.com/bitcoin-core/secp256k1/blob/6ad5cdb42a1a8257289a0423d644dcbdeab0f83c/src/tests.c#L2160
	test_ec_combine<A>(pa, pas, pfs);

	return std::make_tuple(ip, pa, paf, pas, pasf, pfs, pfsf);
}

template <typename A, typename B>
void dumpJSON (
	std::ostream& o,
	const A& compressed,
	const B& uncompressed,
	const std::vector<PC>& pc
) {
	const auto jIP = [](auto x) {
		return jsonifyO({
			x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
			jsonp("P", jsonify(x.a)),
			jsonp("expected", jsonify(x.e))
		});
	};
	const auto jPA = [](auto x) {
		return jsonifyO({
			x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
			jsonp("P", jsonify(x.a)),
			jsonp("Q", jsonify(x.b)),
			x.except.empty() ? jsonp("expected", isNull(x.e) ? "null" : jsonify(x.e)) : "",
			x.except.empty() ? "" : jsonp("exception", jsonify(x.except)),
		});
	};
	const auto jPAS = [](auto x) {
		return jsonifyO({
			x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
			jsonp("P", jsonify(x.a)),
			jsonp("d", jsonify(x.b)),
			x.except.empty() ? jsonp("expected", isNull(x.e) ? "null" : jsonify(x.e)) : "",
			x.except.empty() ? "" : jsonp("exception", jsonify(x.except))
		});
	};
	const auto jPFS = [](auto x) {
		return jsonifyO({
			x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
			jsonp("d", jsonify(x.a)),
			x.except.empty() ? jsonp("expected", isNull(x.e) ? "null" : jsonify(x.e)) : "",
			x.except.empty() ? "" : jsonp("exception", jsonify(x.except)),
		});
	};

	o << jsonifyO({
		jsonp("valid", jsonifyO({
			jsonp("isPoint", jsonifyA({
				jsonify_csv(std::get<0>(compressed), jIP),
				jsonify_csv(std::get<0>(uncompressed), jIP)
			})),
			jsonp("pointAdd", jsonifyA({
				jsonify_csv(std::get<1>(compressed), jPA),
				jsonify_csv(std::get<1>(uncompressed), jPA)
			})),
			jsonp("pointAddScalar", jsonifyA({
				jsonify_csv(std::get<3>(compressed), jPAS),
				jsonify_csv(std::get<3>(uncompressed), jPAS)
			})),
			jsonp("pointFromScalar", jsonifyA({
				jsonify_csv(std::get<5>(compressed), jPFS),
				jsonify_csv(std::get<5>(uncompressed), jPFS)
			})),
			jsonp("pointCompress", jsonifyA(pc, [](auto x) {
				return jsonifyO({
					jsonp("P", jsonify(x.a)),
					jsonp("compress", jsonify(x.b)),
					jsonp("expected", x.e.empty() ? "null" : jsonify(x.e))
				});
			}))
		})),
		jsonp("invalid", jsonifyO({
			jsonp("pointAdd", jsonifyA({
				jsonify_csv(std::get<2>(compressed), jPA),
				jsonify_csv(std::get<2>(uncompressed), jPA)
			})),
			jsonp("pointAddScalar", jsonifyA({
				jsonify_csv(std::get<4>(compressed), jPAS),
				jsonify_csv(std::get<4>(uncompressed), jPAS)
			})),
			jsonp("pointFromScalar", jsonifyA({
				jsonify_csv(std::get<6>(compressed), jPFS),
				jsonify_csv(std::get<6>(uncompressed), jPFS)
			}))
		}))
	});
}

int main () {
	_ec_init();

	const auto c = generate<uint8_t_33>(GENERATORC);
	const auto u = generate<uint8_t_65>(GENERATOR);
	const auto t = generatePC();
	dumpJSON(std::cout, c, u, t);

	return 0;
}
