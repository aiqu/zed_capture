#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <tuple>
#include <queue>
#include <mutex>

#include <opencv2/opencv.hpp>

#include "atomicops.h"
#include "readerwriterqueue.h"

using namespace std;
using namespace moodycamel;

//ReaderWriterQueue<tuple<unsigned long long, cv::Mat>> q(1000);
queue<tuple<unsigned long long, cv::Mat>> q;

atomic<bool> running;
atomic<int> width;
atomic<int> height;

mutex m;

void single(){
	int initial_skip = 60;
    cv::VideoCapture zed_cap(0);

	std::cout << zed_cap.set(cv::CAP_PROP_FRAME_WIDTH, 2560) << "x" << zed_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720) << std::endl;
	width = zed_cap.get(cv::CAP_PROP_FRAME_WIDTH);
	height = zed_cap.get(cv::CAP_PROP_FRAME_HEIGHT);
	std::cout << width << "x" << height << std::endl;
	cout << zed_cap.get(cv::CAP_PROP_FPS) << endl;
	cout << zed_cap.set(cv::CAP_PROP_FPS, 30) << endl;
	cout << zed_cap.get(cv::CAP_PROP_FPS) << endl;

	cv::Mat sbs;
	timespec ts;
	while (zed_cap.read(sbs)){
		if (initial_skip-- < 0)
			break;
	}
	cv::Rect left_rect(0, 0, width / 2, height);
	cv::Rect right_rect(width / 2, 0, width / 2, height);
	FILE *out = fopen("times.txt", "w");
	int idx=0;
    while (zed_cap.read(sbs)) {
		//cv::imshow("View", sbs);
		//cv::waitKey(1);
		clock_gettime(CLOCK_REALTIME, &ts);
		unsigned long long tstamp = (unsigned long long)(ts.tv_sec) * 1e9 + (unsigned long long)(ts.tv_nsec);
		char buf[100];
		sprintf(buf, "%llu.jpg", tstamp);
		cv::imwrite(buf, sbs);
		//fprintf(out, "%llu\n", tstamp);

		//char lname[100], rname[100];
		//sprintf(lname, "left/%llu.jpg", tstamp);
		//sprintf(rname, "right/%llu.jpg", tstamp);
		//cv::Mat left_image = sbs(left_rect);
		//cv::Mat right_image = sbs(right_rect);
		//cv::imwrite(lname, left_image);
		//cv::imwrite(rname, right_image);
		if(!running){
			break;
		}
    }
}

void job(){
	int initial_skip = 60;
    cv::VideoCapture zed_cap(0);

	std::cout << zed_cap.set(cv::CAP_PROP_FRAME_WIDTH, 2560) << "x" << zed_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720) << std::endl;
	width = zed_cap.get(cv::CAP_PROP_FRAME_WIDTH);
	height = zed_cap.get(cv::CAP_PROP_FRAME_HEIGHT);
	std::cout << width << "x" << height << std::endl;
	cout << zed_cap.get(cv::CAP_PROP_FPS) << endl;
	cout << zed_cap.set(cv::CAP_PROP_FPS, 30) << endl;
	cout << zed_cap.get(cv::CAP_PROP_FPS) << endl;


	cv::Mat sbs;
	timespec ts;
	while (zed_cap.read(sbs)){
		if (initial_skip-- < 0)
			break;
	}
    while (zed_cap.read(sbs)) {
		clock_gettime(CLOCK_REALTIME, &ts);
		unsigned long long tstamp = (unsigned long long)(ts.tv_sec) * 1e9 + (unsigned long long)(ts.tv_nsec);
		//q.enqueue(make_tuple(tstamp, sbs.clone()));
		cv::Mat tmp = sbs.clone();
		m.lock();
		q.push(make_tuple(tstamp, tmp));
		m.unlock();
		tmp.release();
		//cv::imshow("SBS", sbs);
		//char c = cv::waitKey(10);
		//if(c=='c'){
			//break;
		//}
		if(!running){
			break;
		}
    }
}

void store(){
	sleep(1);
	while(width == 0 || height == 0){
		cout << "width and height is not initialized" << endl;
	}
	cv::Rect left_rect(0, 0, width / 2, height);
	cv::Rect right_rect(width / 2, 0, width / 2, height);
	FILE *out = fopen("times.txt", "w");
	tuple<unsigned long long, cv::Mat> dat;
	while(running){
		//if(q.try_dequeue(dat)){
		if(!q.empty()){
			m.lock();
			dat = q.front();
			q.pop();
			m.unlock();
			fprintf(out, "%llu\n", get<0>(dat));

			char lname[100], rname[100];
			sprintf(lname, "left/%llu.png", get<0>(dat));
			sprintf(rname, "right/%llu.png", get<0>(dat));
			cv::Mat left_image = get<1>(dat)(left_rect);
			cv::Mat right_image = get<1>(dat)(right_rect);
			cv::imwrite(lname, left_image);
			cv::imwrite(rname, right_image);
			left_image.release();
			right_image.release();
			(&get<1>(dat))->release();
			//cout << q.size_approx() << " item remains" << endl;
			cout << q.size() << " item remains" << endl;
		}
	}
	//cout << "Start consume remaining " << q.size_approx() << " items" << endl;
	cout << "Start consume remaining " << q.size() << " items" << endl;
	//while(q.try_dequeue(dat)){
	while(!q.empty()){
		dat = q.front();
		q.pop();
		fprintf(out, "%llu\n", get<0>(dat));

		char lname[100], rname[100];
		sprintf(lname, "left/%llu.png", get<0>(dat));
		sprintf(rname, "right/%llu.png", get<0>(dat));
		cv::Mat left_image = get<1>(dat)(left_rect);
		cv::Mat right_image = get<1>(dat)(right_rect);
		cv::imwrite(lname, left_image);
		cv::imwrite(rname, right_image);
		left_image.release();
		right_image.release();
		(&get<1>(dat))->release();
		//cout << "\r" << q.size_approx() << " item remains";
		cout << "\r" << q.size() << " item remains";
		fflush(stdout);
	}
	fclose(out);
	cout << endl;
}

int main(int argc, char **argv) {
	running = true;
	//thread capture(job);
	//thread consume(store);
	thread s(single);
	
	char c;
	while(c = getchar()){
		if(c=='c'){
			running=false;
			s.join();
			//capture.join();
			//consume.join();
			break;
		}
	}
	return 0;
}
