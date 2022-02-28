#include "parallel.h"
#include "stdio.h"
#include <iostream>

void parallel_process(cv::Mat in, cv::Mat out, std::function<void(cv::Mat, cv::Mat)> func, int threads) {
	cv::parallel_for_(cv::Range(0, threads), [&] (const cv::Range& range) {
		for (int i = range.start; i < range.end; i ++) {
			int top_row = in.rows * i / threads;
			// done this way to stop rounding errors causing missed rows
			int bottom_row = (in.rows * (i + 1) / threads) - top_row;
			cv::Rect sub_rect(0, top_row, in.cols, bottom_row);

			cv::Mat sub_in(in, sub_rect);
			cv::Mat sub_out(out, sub_rect);

			func(sub_in, sub_out);
		}
	});
}
