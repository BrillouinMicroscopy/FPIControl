#ifndef PDH_H
#define PDH_H

#include <cmath>
#include <complex>
#include "generalmath.h"

class PDH {
	public:
		template <typename T = double>
		static T getError(std::vector<T> data, std::vector<T> reference) {
			
			std::vector<T> tmp(data.size());
			
			std::transform(data.begin(), data.end(),
				reference.begin(), tmp.begin(),
				std::multiplies<T>()
			);
			
			return generalmath::mean(tmp);
		};
};

#endif // PDH_H