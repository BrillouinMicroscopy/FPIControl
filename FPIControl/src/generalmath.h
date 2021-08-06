#ifndef GENERALMATH_H
#define GENERALMATH_H

#include <cstdlib>
#include <numeric>
#include <vector>
#include <array>
#include <complex>
#include <cmath>
#include <algorithm>

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
		std::vector<T>::iterator result = std::max_element(std::begin(vector), std::end(vector));
		return *result;
	}

	template <typename T = double>
	static T min(std::vector<T> vector) {
		std::vector<T>::iterator result = std::min_element(std::begin(vector), std::end(vector));
		return *result;
	}

	template <typename T = double>
	static T absSum(std::vector<T> vector) {
		T sum{ 0 };
		for (int jj{ 0 }; jj < vector.size(); jj++) {
			sum += abs(vector[jj]);
		}
		return sum;
	}

	static double floatingMean(std::vector<double> vector, size_t nrValues, size_t offset = 0) {
		// If we request a floating mean over all or more elements, just return the global mean.
		if (nrValues >= vector.size()) {
			return mean(vector);
		}
		// Floating mean over no element is NaN.
		if (nrValues == 0) {
			return nan("1");
		}

		// If the index of the first element is positive we only need one accumulator
		if (vector.size() >= offset + nrValues) {
			return std::accumulate(std::prev(vector.end(), offset + nrValues), std::prev(vector.end(), offset), 0.0) / nrValues;
		// Else, we wrap around and also average elements from the end of the vector
		} else {
			return (
					std::accumulate(vector.begin(), std::prev(vector.end(), offset), 0.0) +
					std::accumulate(std::prev(vector.end(), nrValues + offset - vector.size()), vector.end(), 0.0)
				) / nrValues;
		}
	}

	static double standardDeviation(std::vector<double> vector) {
		if (vector.size() == 0) {
			return nan("1");
		}
		double m = mean(vector);
		double accum{ 0.0 };
		std::for_each(std::begin(vector), std::end(vector), [&](const double d) {
			accum += (d - m) * (d - m);
		});

		return sqrt(accum / (vector.size() - 1));
	}

	static double floatingStandardDeviation(std::vector<double> vector, size_t nrValues, size_t offset = 0) {
		// If we request a floating standard deviation over all or more elements, just return the global standard deviation.
		if (nrValues >= vector.size()) {
			return standardDeviation(vector);
		}
		// Floating standard deviation over no element is NaN.
		if (vector.size() == 0) {
			return nan("1");
		}
		double m = floatingMean(vector, nrValues, offset);
		auto std_sum = [&](double a, double value) {
			return std::move(a) + ((value - m) * (value - m));
		};
		auto accum{ 0.0 };
		// If the index of the first element is positive we only need one accumulator
		if (vector.size() >= offset + nrValues) {
			accum = std::accumulate(std::prev(vector.end(), offset + nrValues), std::prev(vector.end(), offset), 0.0, std_sum);
		} else {
			accum = std::accumulate(vector.begin(), std::prev(vector.end(), offset), 0.0, std_sum) +
				std::accumulate(std::prev(vector.end(), nrValues + offset - vector.size()), vector.end(), 0.0, std_sum);
		}

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