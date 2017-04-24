#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <tuple>

#include <opencv2/opencv.hpp>

#include "atomicops.h"
#include "readerwriterqueue.h"

using namespace std;
using namespace moodycamel;

ReaderWriterQueue<tuple<unsigned long long, cv::Mat>> q(1000);

atomic<bool> running;
atomic<int> width;
atomic<int> height;

void job(){
	int initial_skip = 60;
    cv::VideoCapture zed_cap(0);

	std::cout << zed_cap.set(cv::CAP_PROP_FRAME_WIDTH, 2560) << "x" << zed_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720) << std::endl;
	width = zed_cap.get(cv::CAP_PROP_FRAME_WIDTH);
	height = zed_cap.get(cv::CAP_PROP_FRAME_HEIGHT);
	std::cout << width << "x" << height << std::endl;
	cout << zed_cap.get(cv::CAP_PROP_FPS) << endl;
	cout << zed_cap.set(cv::CAP_PROP_FPS, 60) << endl;
	cout << zed_cap.get(cv::CAP_PROP_FPS) << endl;


	cv::Mat sbs;
	cv::Mat dummy(10,10,CV_8UC1);
	timespec ts;
	char c;
	while (zed_cap.read(sbs)){
		if (initial_skip-- < 0)
			break;
	}
    while (zed_cap.read(sbs)) {
		clock_gettime(CLOCK_REALTIME, &ts);
		unsigned long long tstamp = (unsigned long long)(ts.tv_sec) * 1e9 + (unsigned long long)(ts.tv_nsec);
		q.enqueue(make_tuple(tstamp, sbs.clone()));
		cv::imshow("SBS", sbs);
		cv::waitKey(10);
		if(!running){
			break;
		}
    }
}

void store(){
	int idx = 0;
	sleep(1);
	while(width == 0 || height == 0){
		cout << "width and height is not initialized" << endl;
	}
	cv::Rect left_rect(0, 0, width / 2, height);
	cv::Rect right_rect(width / 2, 0, width / 2, height);
	FILE *out = fopen("times.txt", "w");
	tuple<unsigned long long, cv::Mat> dat;
	while(running){
		if(q.try_dequeue(dat)){
			fprintf(out, "%05d.png %05d.png %llu\n", idx, idx, get<0>(dat));

			char lname[100], rname[100];
			sprintf(lname, "left/%04d.png", idx);
			sprintf(rname, "right/%04d.png", idx);
			cv::Mat left_image = get<1>(dat)(left_rect);
			cv::Mat right_image = get<1>(dat)(right_rect);
			cv::imwrite(lname, left_image);
			cv::imwrite(rname, right_image);

			idx++;
		}
	}
	cout << "Start consume remaining " << q.size_approx() << " items" << endl;
	while(q.try_dequeue(dat)){
		fprintf(out, "%05d.png %05d.png %llu\n", idx, idx, get<0>(dat));

		char lname[100], rname[100];
		sprintf(lname, "left/%04d.png", idx);
		sprintf(rname, "right/%04d.png", idx);
		cv::Mat left_image = get<1>(dat)(left_rect);
		cv::Mat right_image = get<1>(dat)(right_rect);
		cv::imwrite(lname, left_image);
		cv::imwrite(rname, right_image);

		idx++;
		cout << "\r" << q.size_approx() << " item remains";
		fflush(stdout);
	}
	fclose(out);
	cout << endl;
}

int main(int argc, char **argv) {
	running = true;
	thread capture(job);
	thread consume(store);

	char c;
	while(c = getchar()){
		if(c=='c'){
			running=false;
			capture.join();
			consume.join();
			break;
		}
	}
	return 0;
}
