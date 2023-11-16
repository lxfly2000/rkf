#include<opencv2/opencv.hpp>
#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>

using namespace cv;
using namespace std;

void showFrame(const string &wn, Mat&om, int x, int y, int w, int h, const string &text, double zoom = 1.0)
{
	if (w < 0)
	{
		w = -w;
		x -= w;
	}
	if (h < 0)
	{
		h = -h;
		y -= h;
	}
	Mat m;
	resize(om, m, Size(), zoom, zoom);
	if (x < 0 || x >= m.cols || y < 0 || y >= m.rows || x + w < 0 || x + w >= m.cols || y + h < 0 || y + h >= m.rows)
		return;
	for (int i = x; i < x + w; i++)
	{
		m.at<Vec3b>(y, i)[0] = 255 - m.at<Vec3b>(y, i)[0];
		m.at<Vec3b>(y, i)[1] = 255 - m.at<Vec3b>(y, i)[1];
		m.at<Vec3b>(y, i)[2] = 255 - m.at<Vec3b>(y, i)[2];
		m.at<Vec3b>(y + h, i)[0] = 255 - m.at<Vec3b>(y + h, i)[0];
		m.at<Vec3b>(y + h, i)[1] = 255 - m.at<Vec3b>(y + h, i)[1];
		m.at<Vec3b>(y + h, i)[2] = 255 - m.at<Vec3b>(y + h, i)[2];
	}
	for (int i = y; i < y + h; i++)
	{
		m.at<Vec3b>(i, x)[0] = 255 - m.at<Vec3b>(i, x)[0];
		m.at<Vec3b>(i, x)[1] = 255 - m.at<Vec3b>(i, x)[1];
		m.at<Vec3b>(i, x)[2] = 255 - m.at<Vec3b>(i, x)[2];
		m.at<Vec3b>(i, x + w)[0] = 255 - m.at<Vec3b>(i, x + w)[0];
		m.at<Vec3b>(i, x + w)[1] = 255 - m.at<Vec3b>(i, x + w)[1];
		m.at<Vec3b>(i, x + w)[2] = 255 - m.at<Vec3b>(i, x + w)[2];
	}
	putText(m, text, Point(1, 31), FONT_HERSHEY_COMPLEX, 1, Vec3b(0, 0, 0));
	putText(m, text, Point(0, 30), FONT_HERSHEY_COMPLEX, 1, Vec3b(0, 255, 255));
	imshow(wn, m);
}

int recognizeKeyframe(string path, int x, int y, int w, int h, double similarity, vector<int>*outframe, double*fps, bool useCUDA)
{
	using namespace cv;
	VideoCapture capture(path);
	if (!capture.isOpened())
	{
		cerr << "Error opening file." << endl;
		return -1;
	}
	const string windowName = "Frame";
	ostringstream statusText;
	namedWindow(windowName);
	*fps = capture.get(CAP_PROP_FPS);
	int totalFrames = capture.get(CAP_PROP_FRAME_COUNT);
	int currentFrame = 1, chain = 0, previousChain = 1;
	Mat recognizeFramesChain[2];
	outframe->push_back(0);
	if (!capture.read(recognizeFramesChain[previousChain]))
		return 0;
	while (capture.read(recognizeFramesChain[chain]))
	{
		//https://www.delftstack.com/zh/howto/python/opencv-compare-images/
		if (recognizeFramesChain[chain].type() != CV_8UC3)
		{
			cerr << "Unsupported frame type.\n";
			return -1;
		}
		double errorL2 = norm(recognizeFramesChain[chain](Rect(x, y, w, h)), recognizeFramesChain[previousChain](Rect(x, y, w, h)), NORM_L2);
		double frameSimilarity = 1 - errorL2 / (w*h);
		if (frameSimilarity < similarity)
			outframe->push_back(currentFrame);
		statusText.str("");
		statusText << "Frame: " << currentFrame << "/" << totalFrames << " Keyframes: " << outframe->size();
		showFrame(windowName, recognizeFramesChain[chain], x, y, w, h, statusText.str());
		previousChain = chain;
		chain = (chain + 1) % 2;
		currentFrame++;
		if (pollKey() != -1)
			break;
	}
	outframe->push_back(currentFrame);
	return 0;
}

string fpsToTime(int frame, double fps)
{
	//00:00:00,000
	char fmt[13] = "";
	int ms = frame * 1000 / fps;
	sprintf_s(fmt, "%02d:%02d:%02d,%02d0", ms / 3600000, (ms / 60000) % 60, (ms / 1000) % 60, (ms % 1000) / 10);//在Aegisub中要舍去一位
	return fmt;
}

int outputKeyframe(const string path, const vector<int>&frame, double fps)
{
	using namespace std;
	ofstream srt(path);
	if (!srt)
	{
		cerr << "Cannot write srt file." << endl;
		return -1;
	}
	for (int i = 1; i < frame.size(); i++)
	{
		srt << i << endl << fpsToTime(frame[i - 1], fps) << " --> " << fpsToTime(frame[i], fps) << endl << "Keyframe " << i << ", Frame: " << frame[i - 1] << " - " << frame[i] << endl << endl;
	}
	return 0;
}

double z = 1.0;
int rectX = 0, rectY = 0, rectW = 0, rectH = 0;

void trackCallback(int pos, void*user)
{
	VideoCapture*v = (VideoCapture*)user;
	int totalFrames = v->get(CAP_PROP_FRAME_COUNT);
	v->set(CAP_PROP_POS_FRAMES, pos);
	Mat m;
	if (v->retrieve(m))
		showFrame("Preview", m, 0, 0, 0, 0, "", z);
}

void mouseCallback(int event, int x, int y, int flags, void* user)
{
	VideoCapture*v = (VideoCapture*)user;
	static int beforePressX = -1, beforePressY = -1;
	ostringstream s;
	Mat m;
	switch (event)
	{
	case EVENT_LBUTTONDOWN:
		beforePressX = x;
		beforePressY = y;
		break;
	case EVENT_LBUTTONUP:
		beforePressX = -1;
		beforePressY = -1;
		break;
	case EVENT_MOUSEWHEEL:
		if (getMouseWheelDelta(flags) > 0)
			z *= 2;
		else
			z /= 2;
		v->retrieve(m);
		s << "Zoom: " << z;
		showFrame("Preview", m, 0, 0, 0, 0, s.str(), z);
		break;
	case EVENT_MOUSEMOVE:
		if (beforePressX != -1)
		{
			int w = x - beforePressX;
			int h = y - beforePressY;
			v->retrieve(m);
			rectX = beforePressX / z;
			rectY = beforePressY / z;
			rectW = w / z;
			rectH = h / z;
			s << "X: " << rectX << " Y: " << rectY << " Width:" << rectW << " Height: " << rectH;
			showFrame("Preview", m, beforePressX, beforePressY, w, h, s.str(), z);
		}
		break;
	}
}

int previewFile(string path)
{
	VideoCapture v(path);
	if (!v.isOpened())
	{
		cerr << "Error opening file." << endl;
		return -1;
	}
	Mat m;
	if (!v.read(m))
	{
		cerr << "Error reading file." << endl;
		return -2;
	}
	cout << path << " :\nFPS: " << v.get(CAP_PROP_FPS);
	cout << "\nWidth: " << m.cols << "\nHeight: " << m.rows << endl;
	const string windowName = "Preview";
	const string trackTime = "Frame";
	namedWindow(windowName);
	int totalFrames = v.get(CAP_PROP_FRAME_COUNT);
	createTrackbar(trackTime, windowName, NULL, totalFrames, trackCallback, &v);
	setMouseCallback(windowName, mouseCallback, &v);
	showFrame(windowName, m, 0, 0, 0, 0, "Left Mouse: Choose Rectangle, Mouse Wheel: Zoom, Keyboard: Close");
	waitKey();
	return 0;
}

int main(int argc, char*argv[])
{
	if (argc < 7)
	{
		cout << "Usage: RKF <file> <X> <Y> <Width> <Height> <Similarity>\nSimilarity: 0.0-1.0, Add a keyframe if similarity is smaller than this value.\n";
		if (argc > 1 && previewFile(argv[1]) == 0)
			cout << "Command: RKF \"" << argv[1] << "\" " << rectX << " " << rectY << " " << rectW << " " << rectH << " <Similarity>" << endl;
		return 1;
	}
	vector<int>outframe;
	double fps = 0;
	int result;
	bool useCUDA = false;
	if (argc >= 8 && atoi(argv[7]) == 1)
		useCUDA = true;
	if (result = recognizeKeyframe(argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atof(argv[6]), &outframe, &fps, useCUDA))
		return result;
	string srtfile = argv[1];
	srtfile.resize(srtfile.rfind('.'));
	srtfile.append("_rkf.srt");
	if (result = outputKeyframe(srtfile, outframe, fps))
		return result;
	cout << "File outputed to: " << srtfile << endl;
	return 0;
}
