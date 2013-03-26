
#include <stdio.h>
#include <iostream>
#include <exception>

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include <opencv2/imgproc/imgproc.hpp>

#include "tinycv.h"

#define DEBUG 0

using namespace cv;

struct Image {
  cv::Mat img;
};

// make box lines eq 0° or 90°
inline Point2f normalize_aspect(Point2f in, Point2f x, Point2f y) {
	Point2f out(in);
	out.y += y.y;
	out.y /= 2;
	out.x += x.x;
	out.x /= 2;
	return out;
}

int MyErrorHandler(int status, const char* func_name, const char* err_msg, const char* file_name, int line, void*) {
	// suppress error msg's
	return 0;
}

std::vector<char> str2vec(std::string str_in) {
	std::vector<char> out(str_in.data(), str_in.data() + str_in.length());
	return out;
}

std::vector<int> search_TEMPLATE(std::string str_scene, std::string str_object) {
	cvSetErrMode(CV_ErrModeParent);
	cvRedirectError(MyErrorHandler);

	std::vector<char> data_scene = str2vec(str_scene);
	std::vector<char> data_object = str2vec(str_object);

	Mat img_scene =  imdecode(Mat(data_scene),  CV_LOAD_IMAGE_COLOR );
	Mat img_object = imdecode(Mat(data_object), CV_LOAD_IMAGE_COLOR );

	if( !img_object.data || !img_scene.data ) {
		std::cerr<< "Error reading images" << std::endl;
		throw(std::exception());
	}

	// Calculate size of result matrix and create it
	int res_width  = img_scene.cols - img_object.cols + 1;
	int res_height = img_scene.rows - img_object.rows + 1;
	Mat res = Mat::zeros(res_height, res_width, CV_32FC1);

	// Perform the matching
	// infos about algorythms: http://docs.opencv.org/trunk/doc/tutorials/imgproc/histograms/template_matching/template_matching.html
	matchTemplate(img_scene, img_object, res, CV_TM_CCOEFF_NORMED);

	// Get minimum and maximum values from result matrix
	double minval, maxval;
	Point  minloc, maxloc;
	minMaxLoc(res, &minval, &maxval, &minloc, &maxloc, Mat());

	#if DEBUG
	Mat res_out;
	normalize( res, res_out, 0, 255, NORM_MINMAX, -1, Mat() );
	if(res.cols == 1 && res.rows == 1) {
		float dataval = res.at<float>(0,0);
		res_out.at<float>(0,0) = dataval * 255;
	}
	res_out.convertTo(res_out, CV_8UC1);
	imwrite("result.ppm", res_out);
	#endif

	// Check if we have a match
	if(maxval > 0.9) {
		#if DEBUG
		rectangle(img_scene, Point(maxloc.x, maxloc.y), Point(maxloc.x + img_object.cols, maxloc.y + img_object.rows), CV_RGB(255,0,0), 3);
		imwrite("debug.ppm", img_scene);
		#endif
		std::vector<int> outvec(4);
		outvec[0] = int(maxloc.x);
		outvec[1] = int(maxloc.y);
		outvec[2] = int(maxloc.x + img_object.cols);
		outvec[3] = int(maxloc.y + img_object.rows);
		return outvec;
	}
	else {
		std::vector<int> outvec(0);
		return outvec;
	}
}

void image_destroy(Image *s)
{
  //printf("destroy\n");
  delete(s);
}

Image *image_read(const char *filename)
{
  Image *image = new Image;
  image->img = imread(filename, CV_LOAD_IMAGE_COLOR);
  if (!image->img.data) {
    std::cout << "Could not open image" << filename << std::endl;
    return 0L;
  }
  return image;
}

bool image_write(Image *s, const char *filename)
{
  printf("image_write");
  imwrite(filename, s->img);
  return true;
}

/* stack overflow license ... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>

std::string str2md5(const char* str, int length) {
    int n;
    MD5_CTX c;
    unsigned char digest[16];
    char out[33];

    MD5_Init(&c);

    while (length > 0) {
        if (length > 512) {
            MD5_Update(&c, str, 512);
        } else {
            MD5_Update(&c, str, length);
        }
        length -= 512;
        str += 512;
    }

    MD5_Final(digest, &c);

    for (n = 0; n < 16; ++n) {
      snprintf(out + n*2, 16*2, 
	       "%02x", (unsigned int)digest[n]);
    }

    return out;
}

static vector<uchar> convert_to_ppm(const Mat &s, int &header_length)
{
  vector<uchar> buf;
  if (!imencode(".ppm", s, buf)) {
    fprintf(stderr, "convert_to_ppm failed\n");
    header_length = 0;
    return buf;
  }
  
  const char *cbuf = reinterpret_cast<const char*> (&buf[0]);
  const char *cbuf_start = cbuf;
  // the perl code removed the header before md5, 
  // so we need to do the same
  cbuf = strchr(cbuf, '\n') + 1; // "P6\n";
  cbuf = strchr(cbuf, '\n') + 1; // "800 600\n";
  cbuf = strchr(cbuf, '\n') + 1; // "255\n";

  header_length = cbuf - cbuf_start;
  return buf;
}

std::string image_checksum(Image *s)
{
  int header_length;
  vector<uchar> buf = convert_to_ppm(s->img, header_length);

  const char *cbuf = reinterpret_cast<const char*> (&buf[0]);
  return str2md5(cbuf + header_length, buf.size() - header_length);
}

Image *image_copy(Image *s)
{
  Image *ni = new Image;
  s->img.copyTo(ni->img);
  return ni;
}

long image_xres(Image *s)
{
  return s->img.cols;
}

long image_yres(Image *s)
{
  return s->img.rows;
}

/* 
 * in the image s replace all pixels in the given range with 0 - in place
 */
void image_replacerect(Image *s, long x, long y, long width, long height)
{
  rectangle(s->img, Rect(x, y, width, height), Scalar(0), CV_FILLED);
}

/* copies the given range into a new image */
Image *image_copyrect(Image *s, long x, long y, long width, long height)
{
  Image *n = new Image;
  n->img = Mat(s->img, Range(y, y+height), Range(x,x+width));
  return n;
}

// in-place op: change all values to 0 (if below threshold) or 255 otherwise
void image_threshold(Image *s, int level)
{
  int header_length;
  vector<uchar> buf = convert_to_ppm(s->img, header_length);

  vector<uchar>::iterator it = buf.begin() + header_length;
  for (; it != buf.end(); ++it) {
    *it = (*it < level) ? 0 : 0xff;
  }
  s->img = imdecode(buf, 1);
}

// return 0 if raw difference is larger than maxdiff (on abs() of channel)
bool image_differ(Image *a, Image *b, unsigned char maxdiff)
{
  cv::Mat diff = abs(a->img - b->img);

  int header_length;
  vector<uchar> buf = convert_to_ppm(diff, header_length);

  vector<uchar>::iterator it = buf.begin() + header_length;
  for (; it != buf.end(); ++it) {
    if (*it > maxdiff) return true;
  }

  return false;
}

vector<float> image_avgcolor(Image *s)
{
  Scalar t = mean(s->img);

  vector<float> f;
  f.push_back(t[2] / 255.0); // Red
  f.push_back(t[1] / 255.0); // Green
  f.push_back(t[0] / 255.0); // Blue

  return f;
}

	// STRLEN slen; unsigned char *cs=SvPV(svs,slen);
	// 	STRLEN rlen; unsigned char *cr=SvPV(svr,rlen);
	// 	// divide by 3 because of 3 byte per pix
	// 	// substract 1 to be able to add color-byte offset by hand
	// 	long slen_pix = slen / 3;
	// 	long rlen_pix = rlen / 3;
	// 	long newlineoffset = svsxlen - svrxlen;
	// 	long svrxlen_check = rlen_pix - 1;
	// 	long i, my_i, j, remaining_sline;
	// 	long byteoffset_s, byteoffset_r;
	// 	long rs, bs, gs, rr, br, gr;
	// 	for(i=0; i<slen_pix; i++) {
	// 		remaining_sline = (svsxlen - (i % svsxlen));
	// 		if ( remaining_sline < svrxlen ) {
	// 			// a refimg line would not fit
	// 			// into remaining selfimg line
	// 			// jump to next line
	// 			i += remaining_sline - 1; // ugly but faster
	// 			continue;
	// 		}
	// 		// refimg does fit in remaining img check?
	// 		my_i = i;
	// 		for(j=0; j<rlen_pix; j++) {
	// 			if (j > 0 && j % svrxlen == 0) {
	// 				// we have reached end of a line in refimg
	// 				// pos 0 in refimg does not mean end of line
	// 				my_i += newlineoffset;
	// 			}
	// 			if (my_i >= slen_pix)
	// 				break;

	// 			byteoffset_s = (my_i+j)*3;
	// 			byteoffset_r = j*3;
	// 			if (
	// 				abs(cs[byteoffset_s+0] - cr[byteoffset_r+0]) > maxdiff ||
	// 				abs(cs[byteoffset_s+1] - cr[byteoffset_r+1]) > maxdiff ||
	// 				abs(cs[byteoffset_s+2] - cr[byteoffset_r+2]) > maxdiff
	// 			) {
	// 				//printf(\"x: %d\\n\", (my_i+j) % svsxlen);
	// 				//printf(\"y: %d\\n\", (my_i+j) / svsxlen);
	// 				//printf(\"byte_offset: %d\\n\", byteoffset_s);
	// 				//printf(\"s: %x - r: %x\\n\", cs[byteoffset_s+0], cr[byteoffset_r+0]);
	// 				//printf(\"break\\n\\n\\n\");
	// 				break;
	// 			}
	// 			if (j == svrxlen_check) {
	// 				// last iteration - refimg processed without break
	// 				// return i which is startpos of match (in pixels)
	// 				return i;
	// 			}
	// 		}
	// 	}
	// 	return -1;

// # in: needle to search [ppm object]
// # out: (x,y) coords if found, undef otherwise
// # inspired by OCR::Naive
// sub search($;$) {
// 	my $self=shift;
// 	my $needle=shift;
// 	my $maxdiff = shift || 40;
// 	my $xneedle=$needle->{xres};
// 	my $xhay=$self->{xres};
	

// 	my $pos = searchC($self->{data}, $needle->{data}, $self->{xres}, $needle->{xres}, $maxdiff);

// 	if ($pos ne -1) {
// 	  my($x,$y)=($pos % $xhay, int($pos/$xhay));
// 	  return [$x, $y, $needle->{xres}, $needle->{yres}];
// 	}
// 	return undef;
// }

//  search_fuzzy($;$) {
// 	my $self = shift;
// 	my $needle = shift;
// 	my $algorithm = shift||'template';
// 	my $pos;
// 	if($algorithm eq 'surf') {
// 		$pos = tinycv::search_SURF($self, $needle);
// 	}
// 	elsif($algorithm eq 'template') {
// 		$pos = tinycv::search_TEMPLATE($self, $needle);
// 	}
// 	# if match pos is (x, y, x, y)
// 	# first point is upper left, second is bottom right
// 	if(scalar(@$pos) ge 2) {
// 		return [$pos->[0], $pos->[1], $needle->{xres}, $needle->{yres}]; # (x, y, rxres, ryres)
// 	}
// 	return undef;
// }

std::vector<int> image_search(Image *s, Image *needle, int maxdiff)
{
  printf("image_search\n");
  std::vector<int> ret;
  return ret;
}

std::vector<int> image_search_fuzzy(Image *s, Image *needle)
{
  printf("image_search_fuzzy\n");
  std::vector<int> ret;
  return ret;
}
