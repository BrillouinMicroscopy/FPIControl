#include "stdafx.h"
#include "..\FPIControl\generalmath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace FPIControlUnitTest {		
	TEST_CLASS(UnitTest1) {
		public:
			// Tests for mean()
			TEST_METHOD(TestMethodMeanInt) {
				std::vector<int> vector;
				vector.push_back(1);
				vector.push_back(2);
				vector.push_back(3);
				Assert::AreEqual(2.0, generalmath::mean(vector));
			}

			TEST_METHOD(TestMethodMeanDouble) {
				std::vector<double> vector;
				vector.push_back(1.0);
				vector.push_back(2.0);
				vector.push_back(3.0);
				Assert::AreEqual(2.0, generalmath::mean(vector));
			}

			TEST_METHOD(TestMethodMeanComplex) {
				std::vector<std::complex<double>> vector;
				vector.push_back(std::complex<double>(1.0, 0.0));
				vector.push_back(std::complex<double>(2.0, 1.0));
				vector.push_back(std::complex<double>(3.0, 2.0));
				std::complex<double> expected = std::complex<double>(2.0, 1.0);
				std::complex<double> actual = generalmath::mean(vector);
				Assert::AreEqual(expected.real(), actual.real());
				Assert::AreEqual(expected.imag(), actual.imag());
			}
			// Tests for floatingMean()
			TEST_METHOD(TestMethodFloatingMeanDoubleExact) {
				std::vector<double> vector = {
					1.0, 2.0, 3.0, 4.0, 5.0,
					6.0, 7.0, 8.0, 9.0, 10.0
				};
				Assert::AreEqual(5.5, generalmath::floatingMean(vector,10));
			}

			TEST_METHOD(TestMethodFloatingMeanDoubleShort) {
				std::vector<double> vector = {
					1.0, 2.0, 3.0, 4.0, 5.0,
					6.0, 7.0, 8.0, 9.0, 10.0
				};
				Assert::AreEqual(8.0, generalmath::floatingMean(vector, 5));
			}

			TEST_METHOD(TestMethodFloatingMeanDoubleLong) {
				std::vector<double> vector = {
					1.0, 2.0, 3.0, 4.0, 5.0,
					6.0, 7.0, 8.0, 9.0, 10.0
				};
				Assert::AreEqual(5.5, generalmath::floatingMean(vector, 15));
			}

			TEST_METHOD(TestMethodFloatingMeanDoubleEmpty) {
				std::vector<double> vector = {};
				Assert::IsTrue(isnan(generalmath::floatingMean(vector, 15)));
			}
			// Tests for standardDeviation()
			TEST_METHOD(TestMethodStandardDeviationDoubleZero) {
				std::vector<double> vector = {
					1.0, 1.0, 1.0
				};
				Assert::AreEqual(0.0, generalmath::standardDeviation(vector));
			}

			TEST_METHOD(TestMethodStandardDeviationDoubleOne) {
				std::vector<double> vector = {
					1.0, 2.0, 3.0
				};
				Assert::AreEqual(1.0, generalmath::standardDeviation(vector));
			}

			TEST_METHOD(TestMethodStandardDeviationDoubleHalf) {
				std::vector<double> vector = {
					1.0, 2.0, 1.0, 2.0, 1.0,
					2.0, 1.0, 2.0, 1.0, 2.0, 1.5
				};
				Assert::AreEqual(0.5, generalmath::standardDeviation(vector));
			}

			TEST_METHOD(TestMethodStandardDeviationDoubleEmpty) {
				std::vector<double> vector = {};
				Assert::IsTrue(isnan(generalmath::standardDeviation(vector)));
			}

			// Tests for floatingStandardDeviation
			TEST_METHOD(TestMethodFloatingStandardDeviationDoubleZeroExact) {
				std::vector<double> vector = {
					1.0, 1.0, 1.0
				};
				Assert::AreEqual(0.0, generalmath::floatingStandardDeviation(vector, 3));
			}

			TEST_METHOD(TestMethodFloatingStandardDeviationDoubleOneLong) {
				std::vector<double> vector = {
					1.0, 2.0, 3.0
				};
				Assert::AreEqual(1.0, generalmath::floatingStandardDeviation(vector, 10));
			}

			TEST_METHOD(TestMethodFloatingStandardDeviationDoubleHalfShort) {
				std::vector<double> vector = {
					0.0, 1.0, 2.0, 1.0, 2.0, 1.0,
					2.0, 1.0, 2.0, 1.0, 2.0, 1.5
				};
				Assert::AreEqual(0.5, generalmath::floatingStandardDeviation(vector, 11));
			}

			TEST_METHOD(TestMethodFloatingStandardDeviationDoubleEmpty) {
				std::vector<double> vector = {};
				Assert::IsTrue(isnan(generalmath::floatingStandardDeviation(vector, 10)));
			}

			// Test for floatingMax<int>
			TEST_METHOD(TestMethodFloatingMaxIntShort) {
				std::vector<int> vector = {
					2, -1, 1, 0
				};
				Assert::AreEqual(1, generalmath::floatingMax(vector, 2));
			}

			TEST_METHOD(TestMethodFloatingMaxIntLong) {
				std::vector<int> vector = {
					2, -1, 1, 0
				};
				Assert::AreEqual(2, generalmath::floatingMax(vector, 10));
			}

			TEST_METHOD(TestMethodFloatingMaxIntExact) {
				std::vector<int> vector = {
					2, -1, 1, 0
				};
				Assert::AreEqual(2, generalmath::floatingMax(vector, 4));
			}

			TEST_METHOD(TestMethodFloatingMaxIntEmpty) {
				std::vector<int> vector = {};
				// no NaN for <int>
				Assert::AreEqual(0, generalmath::floatingMax(vector, 2));
			}

			// Test for floatingMax<double>
			TEST_METHOD(TestMethodFloatingMaxDoubleShort) {
				std::vector<double> vector = {
					2.0, -1.0, 1.0, 0.0
				};
				Assert::AreEqual(1.0, generalmath::floatingMax(vector, 2));
			}

			TEST_METHOD(TestMethodFloatingMaxDoubleLong) {
				std::vector<double> vector = {
					2.0, -1.0, 1.0, 0.0
				};
				Assert::AreEqual(2.0, generalmath::floatingMax(vector, 10));
			}

			TEST_METHOD(TestMethodFloatingMaxDoubleExact) {
				std::vector<double> vector = {
					2.0, -1.0, 1.0, 0.0
				};
				Assert::AreEqual(2.0, generalmath::floatingMax(vector, 4));
			}

			TEST_METHOD(TestMethodFloatingMaxDoubleEmpty) {
				std::vector<double> vector = {};
				Assert::IsTrue(isnan(generalmath::floatingMax(vector, 2)));
			}

			TEST_METHOD(TestMethodMinDouble) {
				std::vector<double> vector = { -1.0, 1.0, 2.0, -6.0 };
				Assert::AreEqual(-6.0, generalmath::min(vector));
			}

			TEST_METHOD(TestMethodMinInt) {
				std::vector<int> vector = { -1, 1, 2, -6 };
				Assert::AreEqual(-6, generalmath::min(vector));
			}

			TEST_METHOD(TestMethodMaxDouble) {
				std::vector<double> vector = { -1.0, 1.0, 2.0, -6.0 };
				Assert::AreEqual(2.0, generalmath::max(vector));
			}

			TEST_METHOD(TestMethodMaxInt) {
				std::vector<int> vector = { -1, 1, 2, -6 };
				Assert::AreEqual(2, generalmath::max(vector));
			}
	};
}