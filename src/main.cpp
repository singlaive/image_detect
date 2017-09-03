#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <iostream>
#include <iterator>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

using std::cout;
using std::endl;

static bool read_line(FILE *infile, char *s)
{
  char *news, *sptr;
  static char line[80]="";

  if ((strlen(line) == 0) || ((sptr = strtok (NULL, " \n\t")) == NULL))
  {
     while ((news = fgets (line, 80, infile)) != NULL)
        if ( (line[0] != '#') && ((sptr = strtok (line, " \n\t")) != NULL) )
           break;

     if (news == 0)
        return false;
  }

  strcpy(s, sptr);
  return true;
}

static bool read_ppm_image(const char * fileName, std::vector<unsigned char>& image, int& width, int& height) {
   FILE* infile = fopen(fileName, "rb");
   if ( infile == NULL )
      return false;

   char s[1024]="";

   fgets(s, 4, infile);
   if(strcmp(s, "P6\n" ) != 0)
      return false;

   bool result=true;
   result = (result && read_line(infile, s)); width  = atoi(s);
   result = (result && read_line(infile, s)); height = atoi(s);
   result = (result && read_line(infile, s)); /*maxval = atoi(s);*/
   if(!result) return 0;

   image.resize(width*height*3);
   if(fread(&image[0], 3, height*width, infile ) != (size_t)(height*width))
      return false;
   

   fclose(infile);
   return true;
}

static bool write_ppm_image(const char* fileName, std::vector<unsigned char> const& image, int width, int height)
{
   FILE *outfile = fopen ( fileName, "wb" );
   if(outfile == 0)
      return false;

   fprintf(outfile, "P6\n#Constructed by Mirriad\n%d %d\n255\n", width, height);
   fwrite(&image[0], 3, height*width, outfile);
   fclose(outfile);
   return true;
}

#define sq(x) ((x)*(x))
#define diff(a1,a2,a3,b1,b2,b3) \
    sqrt(sq(a1-b1)+sq(a2-b2)+sq(a3-b3))

static int match_norm(std::vector<unsigned char>::iterator begin, int length_pixal)
{
    int match_merit = 0;
    std::vector<unsigned char>::iterator offset = begin;

    for (int i = 0; i < length_pixal; ++i)
    {
        std::vector<unsigned char>::iterator start = offset + 750*i;

        for (std::vector<unsigned char>::iterator iter = start; iter != start+length_pixal*3; iter+=3)
        {
            cout << int(*iter) << "," << int(*(iter+1)) << "," << int(*(iter+2)) << " ";
            int v = diff(int(*iter), int(*(iter+1)), int(*(iter+2)), 255, 0, 0);
            cout << v << endl;
            match_merit += v;
        }
    }
    match_merit = match_merit/length_pixal/length_pixal;
    return match_merit;
}

class shape_base {};

class rectangular: public shape_base
{
public:
    rectangular(int len_cols, int len_rows, int left_corner_x=0, int left_corner_y=0): x(left_corner_x), y(left_corner_y), cols(len_cols), rows(len_rows) {}
    void fillRGB(int R, int G, int B)
    {
        red_fill = R;
        green_fill = G;
        blue_fill = B; 
    }
    int x;
    int y;
    int cols;
    int rows;
    int red_fill;
    int green_fill;
    int blue_fill;
};

class image_detector_base
{
public:
    virtual rectangular* detect(std::string target_image_path) {}
};

class image_detector_similarity: public image_detector_base
{
public:
    image_detector_similarity(rectangular& template_shape): tmplt_shape(template_shape) {}
    rectangular* detect(const std::string target_image_path)
    {
        int width, height;
        rectangular *p_match = 0;

        (void)read_ppm_image(target_image_path.c_str(), content, width, height);
        std::pair<int, int> point;

        std::vector<int> matches;
        for (int i = 0; i < height-tmplt_shape.cols; ++i)
        {
            for (int j = 0; j < width-tmplt_shape.rows; ++j)
            {
                point.first = j;
                point.second = i;
                matches.push_back(match_norm(point, tmplt_shape.cols, tmplt_shape.rows));
            }
        }

        auto smallest = std::min_element(matches.begin(), matches.end());  
        std::cout << "min element is " << *smallest << " at position " << std::distance(matches.begin(), smallest) << std::endl;  

        int y = std::distance(matches.begin(), smallest)/(width-tmplt_shape.rows);
        int x = std::distance(matches.begin(), smallest) % (width-tmplt_shape.rows);
        cout << "coordinate: " << x << " " << y << endl;
        p_match = new rectangular(tmplt_shape.rows, tmplt_shape.cols, x, y);
        return p_match;
    }
private:
    int match_norm(std::pair<int,int>& point, int colms, int rows)
    {
        int match_merit = 0;
        std::vector<unsigned char>::iterator offset = content.begin()+point.second*750+point.first*3;

        for (int i = 0; i < rows; ++i)
        {
            std::vector<unsigned char>::iterator start = offset + 750*i;
            for (std::vector<unsigned char>::iterator iter = start; iter != start+colms*3; iter+=3)
            {
                int v = diff(int(*iter), int(*(iter+1)), int(*(iter+2)), 255, 0, 0);
                match_merit += v;
            }
        }
        match_merit = match_merit/colms/rows;
        return match_merit;
    }
    rectangular& tmplt_shape;
    std::vector<unsigned char> content;
};

class image_detector_CV: public image_detector_base
{
public:
    image_detector_CV(const std::string &template_image_path = "", int method=0): tmplt_path(template_image_path), match_method(method)
    {
        templt = cv::imread(template_image_path, 1);
    }

    rectangular* detect(std::string target_image_path)
    {
        cv::Mat img, match_result;
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::Point matchLoc;
        rectangular* p_match = 0;
        int result_cols, result_rows;

        img = cv::imread(target_image_path);
        result_cols =  img.cols - templt.cols + 1;
        result_rows = img.rows - templt.rows + 1;
        match_result.create(result_rows, result_cols, CV_32FC1);

        cv::matchTemplate(img, templt, match_result, CV_TM_SQDIFF);
        cv::normalize(match_result, match_result, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());

        cv::minMaxLoc(match_result, &minVal, &maxVal, &minLoc, &maxLoc, cv::Mat());

        if (minVal != maxVal)
        {
            matchLoc = minLoc;
            p_match = new rectangular(matchLoc.x, matchLoc.y, templt.rows, templt.cols);
        }

        return p_match;
    }

private:
    const std::string& tmplt_path;
    cv::Mat templt;
    int match_method;
};

class image_render_base
{
public:
    image_render_base(std::string& image_path): path_image_in(image_path) {}
    virtual image_render_base& set_overlay_rect(rectangular& rect, std::string& overlay_path) {}
    virtual void to(std::string& image_path) {}
protected:
    std::string path_image_in;
    std::string path_image_out;
};

class image_render_similarity: public image_render_base
{
public:
    image_render_similarity(std::string& image_path): image_render_base(image_path)
    {
        
        (void)read_ppm_image(image_path.c_str(), content, width, height);
    }

    image_render_base& set_overlay_rect(rectangular& rect, std::string& overlay_path)
    {
        std::vector<unsigned char> overlay_content;
        int overlay_width, overlay_height;

        (void)read_ppm_image(overlay_path.c_str(), overlay_content, overlay_width, overlay_height);

        std::pair<int, int> point;

        for (int i = 0; i < overlay_height; ++i)
        {
            int overlay_offset = overlay_width * i * 3;
            int content_offset = ((rect.y + i ) * width + rect.x) * 3;
            std::copy(overlay_content.begin()+overlay_offset, overlay_content.begin()+overlay_offset+overlay_width*3, content.begin()+content_offset);
        }

        return *this;
    }

    void to(std::string& image_path)
    {
        (void)write_ppm_image(image_path.c_str(), content, width, height);
    }
private:
    std::vector<unsigned char> content;
    int width, height;
};

class image_render_CV: public image_render_base
{
public:
    image_render_CV(std::string& image_path): image_render_base(image_path)
    {
        img = cv::imread(image_path);
    }

    image_render_base& set_overlay_rect(rectangular& rect, std::string& overlay_path)
    {
        cv::Mat img_overlay = cv::imread(overlay_path);

        img_overlay.copyTo(img(cv::Rect(rect.x,rect.y,rect.cols,rect.rows)));
        return *this;
    }

    void to(std::string& image_path)
    {
        cv::imwrite(image_path, img);
    }
private:
    cv::Mat img;
};
extern char *optarg;
static void display_help(void)
{
    cout << "Usage: imageDetect -r path/to/replacement/image [-t path/to/template/iamge] [-o path/to/output/image] path/to/target/image" << endl;
    cout << "\t* -h prints this help" << endl;
    cout << "\t* Size of template and replacement images must be identical" << endl;
    cout << "\t* Without output path specified, output.ppm will be generated in the current foler" << endl;
    cout << "\t* Without path to template image, you choose to use default one: a red square with side length 50pixel" << endl;

}
/** @function main */
int main( int argc, char** argv )
{
    const char *optString = "o:t:r:h";
    rectangular *p_match = 0;
    std::string overlay_path, tmplt_path, target_path;
    std::string output_path("./output.ppm");
    int opt ;

    if (argc < 2)
    {
        cout << "Too few arguments." << endl;
        exit(-1);
    }

    while ((opt = getopt( argc, argv, optString))!= -1)
    {
        switch (opt)
        {
            case 'o':
                output_path = std::string(optarg);
                break;
            case 't':
                tmplt_path = std::string(optarg);
                break;
            case 'r':
                overlay_path = std::string(optarg);
                break;
            case 'h':
                display_help();
                exit(0);
            default:
                cout << "Invalid!" << endl;
                break;  
        }
    }
     if (optind >= argc) {
        cout << "error" << endl;
    }
    target_path = argv[optind];

    if (overlay_path.empty() || target_path.empty())
    {
        cout << "Argument error" << endl;
        exit(-1);
    }

    image_detector_base *p_detector = 0;
    image_render_base *p_render = 0;

    if (tmplt_path.empty())
    {
        int col = 50;
        int row = 50;
        rectangular rec(col, row);
        rec.fillRGB(255, 0, 0);
        p_detector = new image_detector_similarity(rec);
        p_render = new image_render_similarity(target_path);
    }
    else
    {
        p_detector = new image_detector_CV(tmplt_path, CV_TM_SQDIFF);
        p_render = new image_render_CV(target_path);
    }

    p_match = p_detector->detect(target_path);
    if (0 != p_match)
    {
        p_render->set_overlay_rect(*p_match, overlay_path).to(output_path);
    }

    return 0;
}