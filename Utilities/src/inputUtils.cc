#include "mtf/Utilities/inputUtils.h"

#ifndef DISABLE_VISP
// #if defined _WIN32
////#define VISP_HAVE_FFMPEG
//#define VISP_HAVE_V4L2
//#define VISP_HAVE_DC1394
//#define VISP_HAVE_OPENCV
//#define VISP_HAVE_OPENCV_VERSION 0x020100
//#endif
#include <visp3/io/vpDiskGrabber.h>
#include <visp3/io/vpVideoReader.h>
#include <visp3/sensor/vpV4l2Grabber.h>
#include <visp3/sensor/vp1394TwoGrabber.h>
#endif

#include <stdio.h>


_MTF_BEGIN_NAMESPACE
namespace utils{
	InputBase::InputBase(char _img_source, string _dev_name, string _dev_fmt,
		string _dev_path, int _n_buffers, bool _invert_seq) : n_frames(0),
		n_buffers(_n_buffers), img_source(_img_source), dev_name(_dev_name),
		dev_fmt(_dev_fmt), dev_path(_dev_path), invert_seq(_invert_seq){

		//printf("InputBase :: img_source: %c\n", img_source);

		if(img_source == SRC_VID){
			if(dev_fmt.empty()){ dev_fmt = "mpg"; }
			if(dev_path.empty()){ dev_path = "."; }
			file_path = dev_path + "/" + dev_name + "." + dev_fmt;
			if(!fs::exists(file_path)){
				throw mtf::utils::InvalidArgument(
					cv::format("InputBase :: Video file %s does not exist", file_path.c_str()));
			}
			n_frames = getNumberOfVideoFrames(file_path.c_str());
		} else if(img_source == SRC_IMG || img_source == SRC_DISK){
			if(dev_fmt.empty()){ dev_fmt = "jpg"; }
			if(dev_path.empty()){ dev_path = "."; }
			std::string img_folder_path = dev_path + "/" + dev_name;
			if(!fs::exists(img_folder_path)){
				throw mtf::utils::InvalidArgument(
					cv::format("InputBase :: Image sequence folder %s does not exist", img_folder_path.c_str()));
			}

			file_path = img_folder_path + "/frame%05d." + dev_fmt;

			n_frames = getNumberOfFrames(file_path.c_str());
		}
		if(invert_seq){
			if(n_frames <= 0){
				throw mtf::utils::InvalidArgument(
					cv::format("InputBase :: Inverted sequence cannot be used without valid frame count"));
			}
			if(img_source == SRC_USB_CAM || img_source == SRC_FW_CAM){
				throw mtf::utils::InvalidArgument(
					cv::format("InputBase :: Inverted sequence cannot be used with live input"));
			}
			printf("Using inverted sequence.\n");
			n_buffers = n_frames;
		}
		const_buffer = n_buffers == 1;

		//printf("dev_name: %s\n", dev_name.c_str());
		//printf("dev_path: %s\n", dev_path.c_str());
		//printf("dev_fmt: %s\n", dev_fmt.c_str());
	}
	bool InputCV::initialize(){
		printf("Initializing OpenCV pipeline...\n");

		n_channels = 3;
		if(img_source == SRC_VID || img_source == SRC_IMG) {
			if(img_source == SRC_VID){
				printf("Opening %s video file: %s\n", dev_fmt.c_str(), file_path.c_str());
				//n_frames = cap_obj.get(CV_CAP_PROP_FRAME_COUNT);
			} else{
				printf("Opening %s image files at %s", dev_fmt.c_str(), file_path.c_str());
				if(n_frames > 0){
					printf(" with %d frames\n", n_frames);
				}
				printf("\n");
			}
			cap_obj.open(file_path);
		} else if(img_source == SRC_USB_CAM) {
			if(dev_path.empty()){
				cap_obj.open(0);
			} else{
				cap_obj.open(atoi(dev_path.c_str()));
			}
		} else {
			printf("Invalid source provided for OpenCV Pipeline: %c\n", img_source);
			return false;
		}
		if(!(cap_obj.isOpened())) {
			printf("OpenCV pipeline could not be initialized successfully\n");
			return false;
		}

		//Mat temp_frame;
		//*cap_obj>>temp_frame;
		//frames_captured++;

		//img_height=temp_frame.rows;
		//img_width=temp_frame.cols;

		img_height = static_cast<int>(cap_obj.get(CV_CAP_PROP_FRAME_HEIGHT));
		img_width = static_cast<int>(cap_obj.get(CV_CAP_PROP_FRAME_WIDTH));

		/*img_height=cap_obj.get(CV_CAP_PROP_FRAME_HEIGHT);
		img_width=cap_obj.get(CV_CAP_PROP_FRAME_WIDTH);*/
		printf("OpenCV pipeline initialized successfully to grab frames of size: %d x %d\n",
			img_width, img_height);
		cv_buffer.resize(n_buffers);
		for(int i = 0; i < n_buffers; ++i){
			cv_buffer[i].create(img_height, img_width, img_type);
		}
		buffer_id = 0;
		frame_id = 0;
		if(invert_seq){
			printf("Reading sequence into buffer....\n");
			for(int i = 0; i < n_buffers; ++i){
				if(!cap_obj.read(cv_buffer[n_buffers - i - 1])){ return false; };
			}
			return true;
		}
		return cap_obj.read(cv_buffer[buffer_id]);
	}
	bool InputCV::update(){
		buffer_id = (buffer_id + 1) % n_buffers;
		++frame_id;
		if(invert_seq){
			return true;
		}
		return cap_obj.read(cv_buffer[buffer_id]);
	}
	void InputCV::remapBuffer(unsigned char** new_addr){
		if(invert_seq){
			for(int i = 0; i < n_buffers; ++i){
				cv_buffer[i].copyTo(cv::Mat(cv_buffer[i].rows, cv_buffer[i].cols, cv_buffer[i].type(), new_addr[i]));
				cv_buffer[i].data = new_addr[i];
			}
		} else{
			for(int i = 0; i < n_buffers; ++i){
				//printf("Remapping CV buffer %d to: %lu\n", i, (unsigned long)new_addr[i]);
				cv_buffer[i].data = new_addr[i];
			}
			buffer_id = -1;
			update();
		}
	}
#ifndef DISABLE_VISP
	InputVP::InputVP(char img_source, string _dev_name, string _dev_fmt,
		string _dev_path, int _n_buffers, int _usb_n_buffers,
		bool _invert_seq ,	VpResUSB _usb_res,	VpFpsUSB _usb_fps,
		VpResFW _fw_res, VpFpsFW _fw_fps) :
		InputBase(img_source, _dev_name, _dev_fmt, _dev_path, _n_buffers, _invert_seq),
		frame_id(0), cap_obj(nullptr), usb_res(_usb_res), usb_fps(_usb_fps),
		fw_res(_fw_res), fw_fps(_fw_fps), usb_n_buffers(_usb_n_buffers){}

	bool InputVP::initialize(){
		printf("Initializing ViSP pipeline...\n");
		n_channels = 3;
		if(img_source == SRC_VID || img_source == SRC_IMG) {
			vpVideoReader *vid_cap = new vpVideoReader;
			vid_cap->setFileName(file_path.c_str());
			//n_frames=vid_cap->getLastFrameIndex();
			if(img_source == SRC_VID){
				printf("Opening %s video file: %s with %d frames\n", dev_fmt.c_str(), file_path.c_str(), n_frames);
			} else{
				printf("Opening %s image files at %s with %d frames\n", dev_fmt.c_str(), file_path.c_str(), n_frames);
			}
			cap_obj.reset(vid_cap);
		} else if(img_source == SRC_DISK) {
			vpDiskGrabber *disk_cap = new vpDiskGrabber;
			disk_cap->setDirectory((dev_path + "/" + dev_name).c_str());
			disk_cap->setBaseName("frame");
			disk_cap->setStep(1);
			disk_cap->setNumberOfZero(5);
			disk_cap->setImageNumber(1);
			disk_cap->setExtension(dev_fmt.c_str());
			cap_obj.reset(disk_cap);
		}
#if defined( VISP_HAVE_V4L2 )
		else if(img_source == SRC_USB_CAM) {
			printf("Opening USB camera %s\n", dev_path.c_str());
			vpV4l2Grabber *v4l2_cap = new vpV4l2Grabber;
			v4l2_cap->setInput(atoi(dev_path.c_str()));
			v4l2_cap->setNBuffers(usb_n_buffers);
			switch(usb_res){
			case VpResUSB::Default:
				break;
			case VpResUSB::res640x480:
				v4l2_cap->setWidth(640);
				v4l2_cap->setHeight(480);
				break;
			case VpResUSB::res800x600:
				v4l2_cap->setWidth(800);
				v4l2_cap->setHeight(600);
				break;
			case VpResUSB::res1024x768:
				v4l2_cap->setWidth(1024);
				v4l2_cap->setHeight(768);
				break;
			case VpResUSB::res1280x720:
				v4l2_cap->setWidth(1280);
				v4l2_cap->setHeight(720);
				break;
			case VpResUSB::res1920x1080:
				v4l2_cap->setWidth(1920);
				v4l2_cap->setHeight(1080);
				break;
			default:
				printf("Invalid resolution provided for ViSP USB pipeline\n");
				return false;
			}
			switch(usb_fps){
			case VpFpsUSB::Default:
				break;
			case VpFpsUSB::fps25:
				v4l2_cap->setFramerate(vpV4l2Grabber::framerate_25fps);
				break;
			case VpFpsUSB::fps50:
				v4l2_cap->setFramerate(vpV4l2Grabber::framerate_50fps);
				break;
			default:
				printf("Invalid frame rate provided for ViSP USB pipeline\n");
				return false;
			}
			cap_obj.reset(v4l2_cap);
		}
#endif
#if defined( VISP_HAVE_DC1394 )
		else if(img_source == SRC_FW_CAM) {
			vp1394TwoGrabber *dc1394_cap = new vp1394TwoGrabber;
#ifndef _WIN32
			printf("Opening FireWire camera with GUID %lu\n", dc1394_cap->getGuid());
#else
			printf("Opening FireWire camera with GUID %llu\n", dc1394_cap->getGuid());
#endif
			switch(fw_res){
			case VpResFW::Default:
				break;
			case VpResFW::res640x480:
				dc1394_cap->setVideoMode(vp1394TwoGrabber::vpVIDEO_MODE_640x480_RGB8);
				break;
			case VpResFW::res800x600:
				dc1394_cap->setVideoMode(vp1394TwoGrabber::vpVIDEO_MODE_800x600_RGB8);
				break;
			case VpResFW::res1024x768:
				dc1394_cap->setVideoMode(vp1394TwoGrabber::vpVIDEO_MODE_1024x768_RGB8);
				break;
			case VpResFW::res1280x960:
				dc1394_cap->setVideoMode(vp1394TwoGrabber::vpVIDEO_MODE_1280x960_RGB8);
				break;
			case VpResFW::res1600x1200:
				dc1394_cap->setVideoMode(vp1394TwoGrabber::vpVIDEO_MODE_1600x1200_RGB8);
				break;
			default:
				printf("Invalid resolution provided for ViSP firewire pipeline\n");
				return false;
			}
			switch(fw_fps){
			case VpFpsFW::Default:
				break;
			case VpFpsFW::fps15:
				dc1394_cap->setFramerate(vp1394TwoGrabber::vpFRAMERATE_15);
				break;
			case VpFpsFW::fps30:
				dc1394_cap->setFramerate(vp1394TwoGrabber::vpFRAMERATE_30);
				break;
			case VpFpsFW::fps60:
				dc1394_cap->setFramerate(vp1394TwoGrabber::vpFRAMERATE_60);
				break;
			case VpFpsFW::fps120:
				dc1394_cap->setFramerate(vp1394TwoGrabber::vpFRAMERATE_120);
				break;
			case VpFpsFW::fps240:
				dc1394_cap->setFramerate(vp1394TwoGrabber::vpFRAMERATE_240);
				break;
			default:
				printf("Invalid frame rate provided for ViSP firewire pipeline\n");
				return false;
			}
			cap_obj.reset(dc1394_cap);
		}
#endif
		else {
			printf("Invalid source provided for ViSP Pipeline: %c\n", img_source);
			return false;
		}
		VPImgType temp_img;
		cap_obj->open(temp_img);

		if(cap_obj->getWidth() == 0 || cap_obj->getHeight() == 0) {
			printf("ViSP pipeline could not be initialized successfully\n");
			return false;
		}
		img_width = cap_obj->getWidth();
		img_height = cap_obj->getHeight();
		printf("ViSP pipeline initialized successfully to grab frames of size: %d x %d\n",
			img_width, img_height);
		vp_buffer.resize(n_buffers);
		for(int i = 0; i < n_buffers; ++i){
			vp_buffer[i].init(img_height, img_width);
		}
		buffer_id = 0;
		frame_id = 0;
		if(invert_seq){
			printf("Reading sequence into buffer....\n");
			for(int i = 1; i < n_buffers; ++i){
				cap_obj->acquire(vp_buffer[n_buffers - i - 1]);
			}
			vp_buffer[n_buffers - 1] = temp_img;
		} else{
			vp_buffer[buffer_id] = temp_img;
		}
		cv_frame.create(img_height, img_width, CV_8UC3);
#ifdef VISP_HAVE_OPENCV
		vpImageConvert::convert(vp_buffer[buffer_id], cv_frame);
#else
		convert(vp_buffer[buffer_id], cv_frame);
#endif		
		return true;
	}
	bool InputVP::update(){
		buffer_id = (buffer_id + 1) % n_buffers;
		++frame_id;
		if(!invert_seq){
			cap_obj->acquire(vp_buffer[buffer_id]);
		}
#ifdef VISP_HAVE_OPENCV
		vpImageConvert::convert(vp_buffer[buffer_id], cv_frame);
#else
		convert(vp_buffer[buffer_id], cv_frame);
#endif
		return true;
	}
	void InputVP::remapBuffer(unsigned char** new_addr){
		for(int i = 0; i < n_buffers; ++i){
			vp_buffer[i].bitmap = (vpRGBa*)(new_addr[i]);
		}
		buffer_id = -1;
		update();
	}
	void InputVP::convert(const VPImgType &vp_img, cv::Mat &cv_img){
		for(unsigned int row_id = 0; row_id < vp_img.getHeight(); ++row_id){
			for(unsigned int col_id = 0; col_id < vp_img.getWidth(); ++col_id){
				vpRGBa vp_val = vp_img(row_id, col_id);
				cv_img.at<cv::Vec3b>(row_id, col_id) = cv::Vec3b(vp_val.B, vp_val.G, vp_val.R);
			}
		}
	}
#endif
#ifndef DISABLE_XVISION
	InputXV24::InputXV24(char img_source,
		string dev_path, string dev_fmt, string file_path,
		int n_buffers_in, int n_frames) : vid(nullptr), n_buffers(n_buffers_in){
		switch(img_source) {
		case SRC_IMG: {
			fprintf(stdout, "Opening jpeg file at %s with %d frames\n", file_path.c_str(), n_frames);
			vid.reset(new IMG(file_path.c_str(), 1, n_frames, n_buffers));
			break;
		}
		case SRC_USB_CAM: {
			fprintf(stdout, "Opening USB camera %s with format %s\n", dev_path.c_str(), dev_fmt.c_str());
			vid.reset(new V4L2(dev_path.c_str(), dev_fmt.c_str()));
			break;
		}
		case SRC_FW_CAM: {
			fprintf(stdout, "Opening FIREWIRE camera %s  with format %s\n", dev_path.c_str(), dev_fmt.c_str());
			vid.reset(new DIG1394(dev_path.c_str(), dev_fmt.c_str(), DIG1394_NTH_CAMERA(0)));
			break;
		}
		default: {
			fprintf(stdout, "Invalid image source provided for 24 bit Xvision pipeline\n");
			exit(0);
		}
		}
		XVSize img_size = vid->get_size();
		img_height = img_size.Height();
		img_width = img_size.Width();
	}
	void InputXV24::initFrame(int init_buffer_id){
		if(vid->initiate_acquire(init_buffer_id) < 0){
			cout << "Error in InputXV24: Frame could not be acquired\n";
		}
	}
	void InputXV24::initBuffer(PIX_TYPE24 **init_addrs,
		IMAGE_TYPE24 *buffer_addrs){
		vid_buffers = vid->remap(init_addrs, n_buffers);
		for(int i = 0; i < n_buffers; i++){
			vid_buffers[i].resize(img_width, img_height);
			vid_buffers[i].remap((PIX_TYPE24*)buffer_addrs[i].data(), false);
		}
	}
	void InputXV24::updateFrame(int buffer_id){
		if(vid->initiate_acquire((buffer_id + 1) % n_buffers) < 0){
			cout << "Error in InputXV24: Frame could not be acquired from the input stream\n";
			return;
		}
		vid->wait_for_completion(buffer_id);
	}
	void InputXV24::updateBuffer(PIX_TYPE24 **new_addrs){
		for(int i = 0; i < n_buffers; i++){
			vid_buffers[i].remap(new_addrs[i], false);
		}
	}
	InputXV32::InputXV32(int img_source, string file_path) : vid(nullptr){
		switch(img_source) {
		case SRC_VID: {
			fprintf(stdout, "Opening video file %s\n", file_path.c_str());
			vid.reset(new MPG(file_path.c_str()));
			break;
		}
		default: {
			fprintf(stdout, "Invalid image source provided for 32 bit Xvision pipeline\n");
			exit(0);
		}
		}
		XVSize img_size = vid->get_size();
		img_height = img_size.Height();
		img_width = img_size.Width();

	}
	void InputXV32::initFrame(int init_buffer_id){
		buffer_id32 = init_buffer_id;
		if(vid->initiate_acquire(init_buffer_id) < 0){
			cout << "Error in InputXV32:: Frame could not be acquired\n";
			return;
		}
	}
	void InputXV32::initBuffer(PIX_TYPE24 **init_addrs,
		IMAGE_TYPE24 *buffer_addrs){
		buffer24 = buffer_addrs;
	}
	// XVMpeg does not support 24 bit images so it has to be used to 
	// capture 32 bit images and then copy data to the 24 bit buffers
	void InputXV32::updateFrame(int buffer_id24){
		// XVMpeg does not provide any option to set the number of buffers so we have to use 
		// the default hard coded there in XVMpeg.h as 'DEF_MPEG_NUM'
		buffer_id32 = (buffer_id32 + 1) % DEF_MPEG_NUM;
		//printf("**************************getFrameMPG**********************\n");
		if(vid->initiate_acquire(buffer_id32) < 0){
			cout << "Error in InputXV32: Frame could not be acquired from the input stream\n";
			return;
		}
		vid->wait_for_completion(buffer_id32);
		copyFrame32ToBuffer24(vid->frame(buffer_id32), buffer_id24);
	}
	// copy image data from 32 bit source image to 24 bit buffer image
	void InputXV32::copyFrame32ToBuffer24(IMAGE_TYPE32 &src_frame, int buffer_id24) {
		unsigned char* src_data = (unsigned char*)(src_frame.data());
		unsigned char* dst_data = (unsigned char*)(buffer24[buffer_id24].data());

		for(int row = 0; row < img_height; row++) {
			for(int col = 0; col < img_width; col++) {
				memcpy(dst_data, src_data, sizeof(PIX_TYPE24));
				dst_data += sizeof(PIX_TYPE24);
				src_data += sizeof(PIX_TYPE32);
			}
		}
	}
	bool InputXV::initialize(){
		if(invert_seq && (img_source == SRC_FW_CAM || img_source == SRC_USB_CAM)){
			printf("InputXV :: Inverted sequence cannot be used with live input");
			return false;
		}

		n_channels = sizeof(PIX_TYPE);

		if(img_source == SRC_VID){
			src.reset(new InputXV32(img_source, file_path));
		} else{
			src.reset(new InputXV24(img_source, dev_path, dev_fmt, file_path, n_buffers, n_frames));
		}

		img_height = src->img_height;
		img_width = src->img_width;

		if(img_height == 0 || img_width == 0){
			return false;
		}

		printf("Xvision pipeline initialized successfully to grab frames of size: %d x %d\n",
			img_width, img_height);

		vector<PIX_TYPE*> data_addrs;
		cv_buffer.resize(n_buffers);
		xv_buffer.resize(n_buffers);
		for(int i = 0; i < n_buffers; ++i){
			cv_buffer[i].create(img_height, img_width, CV_8UC3);
			xv_buffer[i] = IMAGE_TYPE(img_width, img_height);
			data_addrs.push_back((PIX_TYPE*)(xv_buffer[i].data()));
			cv_buffer[i].data = (unsigned char*)xv_buffer[i].data();
		}
		src->initBuffer(data_addrs.data(), xv_buffer.data());
		buffer_id = 0;
		frame_id = 0;
		if(invert_seq){
			printf("Reading sequence into buffer....\n");
			for(int i = 0; i < n_buffers; ++i){
				src->updateFrame(n_buffers - i - 1);
			}
			return true;
		}
		src->updateFrame(buffer_id);
		return true;
	}
	bool InputXV::update(){
		++frame_id;
		buffer_id = (buffer_id + 1) % n_buffers;
		if(!invert_seq){
			src->updateFrame(buffer_id);
		}
		return true;
	}
	void InputXV::remapBuffer(unsigned char** new_addr){
		//vid->own_buffers=0;
		//vid_buffers=vid->remap((PIX_TYPE**)new_addr, n_buffers);
		src->updateBuffer((PIX_TYPE**)new_addr);
		for(int i = 0; i < n_buffers; ++i){
			//vid_buffers[i].resize(img_width, img_height);			
			/*IMAGE_TYPE* temp_frame=&(vid->frame(i));

			//temp_frame->pixmap->own_flag=false;
			temp_frame->resize(img_width, img_height);
			temp_frame->remap((PIX_TYPE*)new_addr[i], false);*/
			xv_buffer[i].remap((PIX_TYPE*)(new_addr[i], false));
			cv_buffer[i].data = new_addr[i];
			//temp_frame->pixmap=new XVPixmap<PIX_TYPE>(img_width, img_height, (PIX_TYPE*)new_addr[i], false);
			//temp_frame->win_addr=temp_frame->pixmap->buffer_addr;
		}
		//vid->remap((PIX_TYPE**)new_addr, n_buffers);
		buffer_id = -1;
		src->initFrame(0);
		update();
	}
	void InputXV::copyXVToCV(IMAGE_TYPE_GS &xv_img, cv::Mat &cv_img) {
		//printf("Copying XV to CV\n");
		int img_height = xv_img.SizeY();
		int img_width = xv_img.SizeX();
		//printf("img_height: %d, img_width=%d\n", img_height, img_width);
		/*cv_img=new cv::Mat(img_height, img_width, CV_8UC1);
		printf("created image with img_height: %d, img_width: %d\n",
		cv_img->rows, cv_img->cols);*/
		PIX_TYPE_GS* xv_data = (PIX_TYPE_GS*)xv_img.data();
		unsigned char* cv_data = cv_img.data;
		for(int row = 0; row < img_height; row++) {
			//printf("row: %d\n", row);
			for(int col = 0; col < img_width; col++) {
				//printf("\tcol: %d\n", col);
				int xv_location = col + row * img_width;
				int cv_location = (col + row * img_width);
				cv_data[cv_location] = (unsigned char)xv_data[xv_location];
			}
		}
		//printf("done\n");
	}
	void InputXV::copyXVToCVIter(IMAGE_TYPE_GS &xv_img, cv::Mat &cv_img) {
		int img_width = xv_img.Width();
		int img_height = xv_img.Height();
		//printf("img_height: %d, img_width=%d\n", img_height, img_width);
		int cv_location = 0;
		XVImageIterator<PIX_TYPE_GS> iter(xv_img);
		unsigned char* cv_data = cv_img.data;
		for(; !iter.end(); ++iter){
			cv_data[cv_location++] = *iter;
		}
	}
	void InputXV::copyXVToCVIter(IMAGE_TYPE &xv_img, cv::Mat &cv_img) {
		int img_width = xv_img.Width();
		int img_height = xv_img.Height();
		//printf("img_height: %d, img_width=%d\n", img_height, img_width);
		int cv_location = 0;
		XVImageIterator<PIX_TYPE> iter(xv_img);
		unsigned char* cv_data = cv_img.data;
		for(; !iter.end(); ++iter){
			cv_data[cv_location++] = iter->b;
			cv_data[cv_location++] = iter->g;
			cv_data[cv_location++] = iter->r;
		}
	}
#endif

	int getNumberOfFrames(const char *file_template){
		int frame_id = 0;
		while(FILE* fid = fopen(cv::format(file_template, ++frame_id).c_str(), "r")){
			fclose(fid);
		}
		return frame_id - 1;
	}

	int getNumberOfVideoFrames(const char *file_name){
		cv::VideoCapture cap_obj(file_name);
		cv::Mat img;
		int frame_id = 0;
		while(cap_obj.read(img)){ ++frame_id; }
		cap_obj.release();
		return frame_id;
	}
}
_MTF_END_NAMESPACE

