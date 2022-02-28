#include <opencv2/opencv.hpp>
#include <functional>

void parallel_process(cv::Mat in, cv::Mat out, std::function<void(cv::Mat, cv::Mat)> func, int threads);
