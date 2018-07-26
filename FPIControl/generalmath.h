#ifndef GENERALMATH_H
#define GENERALMATH_H

#include <cstdlib>
#include <numeric>
#include <vector>
#include <array>
#include <complex>
#include <cmath>

class generalmath {
public:
	static double mean(std::vector<int> vector) {
		return std::accumulate(std::begin(vector), std::end(vector), 0.0) / vector.size();
	}

	static double mean(std::vector<double> vector) {
		return std::accumulate(std::begin(vector), std::end(vector), 0.0) / vector.size();
	}

	static std::complex<double> mean(std::vector<std::complex<double>> vector) {
		return std::accumulate(std::begin(vector), std::end(vector), std::complex<double>(0.0, 0.0)) / std::complex<double>(vector.size(), 0);
	}

	template <typename T = double>
	static T max(std::vector<T> vector) {
		T max = vector[0];
		for (int jj(0); jj < vector.size(); jj++) {
			if (vector[jj] > max) {
				max = vector[jj];
			}
		}
		return max;
	}

	template <typename T = double>
	static T min(std::vector<T> vector) {
		T min = vector[0];
		for (int jj(0); jj < vector.size(); jj++) {
			if (vector[jj] < min) {
				min = vector[jj];
			}
		}
		return min;
	}

	template <typename T = double>
	static T absSum(std::vector<T> vector) {
		T sum = 0;
		for (int jj(0); jj < vector.size(); jj++) {
			sum += abs(vector[jj]);
		}
		return sum;
	}

	static double floatingMean(std::vector<double> vector, size_t nrValues) {
		nrValues = (nrValues > vector.size()) ? vector.size() : nrValues;
		if (nrValues == 0) {
			return nan("1");
		}
		return std::accumulate(std::prev(std::end(vector), nrValues), std::end(vector), 0.0) / nrValues;
	}

	static double standardDeviation(std::vector<double> vector) {
		if (vector.size() == 0) {
			return nan("1");
		}
		double m = mean(vector);
		double accum = 0.0;
		std::for_each(std::begin(vector), std::end(vector), [&](const double d) {
			accum += (d - m) * (d - m);
		});

		return sqrt(accum / (vector.size() - 1));
	}

	static double floatingStandardDeviation(std::vector<double> vector, size_t nrValues) {
		if (vector.size() == 0) {
			return nan("1");
		}
		nrValues = (nrValues > vector.size()) ? vector.size() : nrValues;
		double m = floatingMean(vector, nrValues);
		double accum = 0.0;
		std::for_each(std::prev(std::end(vector), nrValues), std::end(vector), [&](const double d) {
			accum += (d - m) * (d - m);
		});

		return sqrt(accum / (nrValues - 1));
	}

	static double floatingMax(std::vector<double> vector, size_t nrValues) {
		if (vector.size() == 0) {
			return nan("1");
		}
		nrValues = (nrValues > vector.size()) ? vector.size() : nrValues;
		double max = *std::prev(std::end(vector), nrValues);
		std::for_each(std::prev(std::end(vector), nrValues), std::end(vector), [&](const double d) {
			if (d > max) {
				max = d;
			}
		});
		return max;
	}

	static int32_t floatingMax(std::vector<int32_t> vector, size_t nrValues) {
		if (vector.size() == 0) {
			// should return NaN, but there is no NaN implementation for int/int32_t
			return 0;
		}
		nrValues = (nrValues > vector.size()) ? vector.size() : nrValues;
		int32_t max = *std::prev(std::end(vector), nrValues);
		std::for_each(std::prev(std::end(vector), nrValues), std::end(vector), [&](const int32_t d) {
			if (d > max) {
				max = d;
			}
		});
		return max;
	}

	// return linearly spaced vector
	template <typename T = double>
	static std::vector<T> linspace(T min, T max, size_t N) {
		T spacing = (max - min) / static_cast<T>(N - 1);
		std::vector<T> xs(N);
		typename std::vector<T>::iterator x;
		T value;
		for (x = xs.begin(), value = min; x != xs.end(); ++x, value += spacing) {
			*x = value;
		}
		return xs;
	}
};

#endif // GENERALMATH_H