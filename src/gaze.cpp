#include <iostream>
#include <iomanip>
#include <fstream>
#include <time.h>

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include "../CrowdSight/src/crowdsight.h"
#include "demo_version.h"

#ifndef HUMAN_NAME
#define HUMAN_NAME "CrowdSight Demo"
#endif

#define RECORDING 0

int main (int argc, char *argv[])
{
    cv::VideoCapture cap;
    std::string auth_key;
    std::string file_name;
    std::string data_dir = "../data/";
    std::string input;
    bool is_webcam_input = false;

    switch (argc)
    {
    case 4:
    {
        file_name = argv[1];
        data_dir  = argv[2];
        auth_key = argv[3];
        input = argv[1];
        cap.open(file_name);
        break;
    }
    case 5:
    {
        file_name = argv[1];
        if (file_name.find("--capture") != std::string::npos)
        {
            cap.open(atoi(argv[2]));
            is_webcam_input = true;
            input = argv[2];
        }
        data_dir = argv[3];
        auth_key = argv[4];
        break;
    }
    default:
    {
        std::cout
                << "Usage: "
                << argv[0]
                << " <videofile> <data dir> <auth key>"
                << std::endl;
        std::cout
                << "       "
                << argv[0]
                << " --capture <frame-id> <data dir> <auth key>"
                << std::endl;
        return -1;
    }
    }

    if (!cap.isOpened())  // check if we can capture from frame
    {
        std::cerr
                << "Couldn't capture video from input "
                << input
                << std::endl;
        return -1;
    }

    //Create crowdsight instance
    CrowdSight crowdsight(data_dir);

    //Authenticate crowdsight instance
    if (!crowdsight.authenticate(auth_key))
    {
        std::cerr << crowdsight.getErrorDescription() << std::endl;
        return -1;
    }

    //Setup video frame resolution
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 480);

    //Create an OpenCV output display window
    cv::namedWindow(HUMAN_NAME);

    cv::Mat frame;

    static CvScalar colors[] =
    {
        {{0, 0, 255}},
        {{0, 128, 255}},
        {{0, 255, 255}},
        {{0, 255, 0}},
        {{255, 128, 0}},
        {{255, 255, 0}},
        {{255, 0, 0}},
        {{255, 0, 255}}
    };

#if RECORDING
    std::ofstream storeFile;
    std::ostringstream fileName;
    fileName << "outputData_" << time(NULL) << ".csv";
    storeFile.open (fileName.str().c_str(), std::ios::out | std::ios::app);

    std::ofstream storeFileBinned;
    std::ostringstream fileNameBinned;
    fileNameBinned << "outputData_" << time(NULL) << "_binned.csv";
    storeFileBinned.open (fileNameBinned.str().c_str(), std::ios::out | std::ios::app);

    int outputBinSizeInMiliSec = 10000;
    int endTimePreviousBin     = 0;
    int binCounter             = 0;
    int agesBinSize            = 20;
    int lastBinsPeopleCounter  = 0;

    std::vector<int> ages   (4,0);
    std::vector<int> genders(2,0);
    std::vector<int> yaws   (3,0);
    std::vector<int> pitches(3,0);
    std::vector<int> uniqueIDsInOutput;
    std::vector<int> newIDsThisBin;
#endif


    //If desirable skips of a video can be skipped.
    int processEveryNthFrame = 1;
    // keep track of how many frames are processed
    int frameCount           = 0;

    cap >> frame;

#if RECORDING
    std::ostringstream videoName;
    videoName << "outputVideo_" << time(NULL) << ".avi"; //videoName.str().c_str()
    cv::VideoWriter videoOutput(videoName.str().c_str(), CV_FOURCC('M','J','P','G'), 5, frame.size());
    bool firstFrameToBeWritten = true;
#endif

    //Start main processing loop
    while(true)
    {
        frameCount++;

        //Grab a frame
        cap >> frame;

        // below defines a subsequence that is nice for demonstration purposes

//        std::cout << frameCount << std::endl;
//        if(frameCount < 775)
//          continue;

//        if(frameCount == 920)
//          break;

        //If frame is empty break
        if (frame.empty())
        {
            break;
        }


        //Flip frame if capturing from webcam
        if (is_webcam_input)
        {
            cv::flip(frame, frame, 1);
        }

        int marginTop     = 0;
        int marginBottom  = 0;
        int marginLeft    = 0;
        int marginRight   = 0;

        cv::Rect roi;
        //specify the roi that will be extracted from the frame
        roi.x      = marginLeft;                    // pixels to skip from the left
        roi.y      = marginTop;                     // pixels to skip from the top
        roi.width  = frame.cols-roi.x-marginRight;  // width of roi
        roi.height = frame.rows-roi.y-marginBottom; // height of roi

        //Use crowdsight instance to procees current frame.
        //Process function evaluates the frames contents and
        //must be called before getCurrentPeople();


        if (frameCount % processEveryNthFrame == 0 && frameCount>0)
        {
            if (!crowdsight.process(frame, roi))
            {
                std::cerr << crowdsight.getErrorDescription() << std::endl;
            }
        }

        //Get the list of people in the current frame
        std::vector<Person> people;
        if (!crowdsight.getCurrentPeople(people))
        {
            std::cerr << crowdsight.getErrorDescription() << std::endl;
        }

        //For each person in the frame, do:
        for (unsigned int i=0; i<people.size();i++)
        {
            //Get person at current index
            Person person = people.at(i);

            //Retrieve the person's face
            cv::Rect face = person.getFaceRect();

            //Retrieve left and right eye locations of the person.
            //Eye location is relative to the face rectangle.
            cv::Point right_eye = person.getRightEye();
            cv::Point left_eye  = person.getLeftEye();

            //Offset eye position with face position, to get frame coordinates.
            right_eye.x += face.x;
            right_eye.y += face.y;
            left_eye.x += face.x;
            left_eye.y += face.y;

            //Draw circles in the center of left and right eyes
            cv::circle(frame, right_eye, 3, cv::Scalar(0,255,0));
            cv::circle(frame, left_eye, 3, cv::Scalar(0,255,0));

            //Get person's ID and other features and draw it in the face rectangle
            std::ostringstream id_string;
            id_string << "ID #" << person.getID() << "/" << person.getPredatorID();
//            std::ostringstream age_string;
//            age_string << "Age: " << person.getAge();

            //Get person's attention span. This value is returned in milliseconds
            std::ostringstream attention_string;
            attention_string << "Attention: " << ( person.getAttentionSpan() / 1000 );

            cv::putText(frame, id_string.str(), cv::Point(face.x+3, face.y+14),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, colors[person.getID()%8]);
//            cv::putText(frame, age_string.str(), cv::Point(face.x+3, face.y+34),
//                        cv::FONT_HERSHEY_SIMPLEX, 0.5, colors[person.getID()%8]);
            cv::putText(frame, attention_string.str(), cv::Point(face.x+3, face.y + face.height-4),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255,255,255));

            // SHOW MOOD BAR
            float moodValue = (person.getMood()+1.)/2.;
            int moodPos     = 16;
            cv::Rect moodBorder = cv::Rect(face.x                                  , face.y + face.height + moodPos    , face.width                                  , 12 );
            cv::Rect moodRed    = cv::Rect(face.x + floor(moodValue*face.width) + 1, face.y + face.height + moodPos + 1, face.width - floor(moodValue*face.width) - 1, 10 );
            cv::Rect moodGreen  = cv::Rect(face.x + 1                              , face.y + face.height + moodPos + 1, moodValue * face.width -1                     , 10 );
            cv::rectangle( frame, moodRed   , cv::Scalar(0,0,255)    , CV_FILLED );
            cv::rectangle( frame, moodGreen , cv::Scalar(0,255,0)    , CV_FILLED );
            cv::rectangle( frame, moodBorder, cv::Scalar(255,255,255), 1 );

            cv::putText(frame, "MOOD", cv::Point(face.x+3, face.y+face.height+moodPos+8),
                        cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(255, 255, 255));

            // SHOW GENDER BAR
            float genderValue = (person.getGender() + 1) / 2.;
            int genderPos = 2;


            cv::Rect genderBorder = cv::Rect(face.x                                  , face.y + face.height + genderPos    , face.width                                  , 12 );
            cv::Rect genderPink   = cv::Rect(face.x + floor(genderValue*face.width) + 1, face.y + face.height + genderPos + 1, face.width - floor(genderValue*face.width) - 1, 10 );
            cv::Rect genderBlue   = cv::Rect(face.x + 1                              , face.y + face.height + genderPos + 1, genderValue * face.width -1                     , 10 );

            cv::rectangle( frame, genderBlue,cv::Scalar(255,55,55), CV_FILLED);
            cv::rectangle( frame, genderPink, cv::Scalar(147,20,255), CV_FILLED );
            cv::rectangle( frame, genderBorder,cv::Scalar(255,255,255), 1 );

            cv::putText(frame, "GENDER", cv::Point( face.x+3, face.y+face.height+genderPos+8 ),
                        cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(255, 255, 255));



            // SHOW AGE BAR
            float ageValue = person.getAge();
            if (ageValue > 80) {ageValue=80;}
            double agePerc = ((double)ageValue/80.0);
            int ageLocInBar = (int)(face.width*agePerc);
            int agePos = 30;
            int ageBlock = (int)face.width/4;

            cv::Rect ageBorder     = cv::Rect( face.x                   , face.y+face.height+agePos  , round(face.width/2) + round(face.width/2), 12 );
            cv::Rect ageGreen      = cv::Rect( face.x + 1               , face.y+face.height+agePos+1, ageBlock, 10 );
            cv::Rect ageOrange     = cv::Rect( face.x + 1 +    ageBlock , face.y+face.height+agePos+1, ageBlock, 10 );
            cv::Rect ageBlue       = cv::Rect( face.x + 1 + (2*ageBlock), face.y+face.height+agePos+1, ageBlock, 10 );
            cv::Rect ageGrey       = cv::Rect( face.x + 1 + (3*ageBlock), face.y+face.height+agePos+1, ageBlock, 10 );
            cv::Rect ageIndicatorB = cv::Rect( face.x + 1 + ageLocInBar - 1, face.y+face.height+agePos+1, 4, 10 );
            cv::Rect ageIndicatorW = cv::Rect( face.x + 1 + ageLocInBar    , face.y+face.height+agePos+1, 2, 10 );

            cv::rectangle( frame, ageGreen     , cv::Scalar(50,205,50)   , CV_FILLED );
            cv::rectangle( frame, ageOrange    , cv::Scalar(249,77,0)    , CV_FILLED );
            cv::rectangle( frame, ageBlue      , cv::Scalar(0,102,255)   , CV_FILLED );
            cv::rectangle( frame, ageGrey      , cv::Scalar(126,126,126) , CV_FILLED );
            cv::rectangle( frame, ageIndicatorB, cv::Scalar(0,0,0)       , CV_FILLED );
            cv::rectangle( frame, ageIndicatorW, cv::Scalar(255,255,255) , CV_FILLED );
            cv::rectangle( frame, ageBorder    , cv::Scalar(255,255,255) , 1 );


//            cv::putText(frame, "AGE", cv::Point(face.x+3, face.y+face.height+moodPos+8),
//                        cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(255, 255, 255));


            // Draw a rectangle around person's face on the current frame
            cv::rectangle(frame, face, cv::Scalar(255,255,255), 1);

            // Draw the continuous measure of mood
//            cv::Rect theMood           = cv::Rect( face.x+round(face.width*moodValue),   face.y+face.height+moodPos+1, 3, 10 );
//            cv::Rect theMoodInnerWhite = cv::Rect( face.x+round(face.width*moodValue)+1, face.y+face.height+moodPos+2, 1,  8 );
//            cv::rectangle(frame,theMood,cv::Scalar(0,0,0),CV_FILLED);
//            cv::rectangle(frame,theMoodInnerWhite,cv::Scalar(255,255,255),CV_FILLED);

            // Draw the continuous measure of gender
//            cv::Rect theGender           = cv::Rect( face.x+round(face.width*(1.-genderValue))  , face.y+face.height+genderPos+1, 3, 10 );
//            cv::Rect theGenderInnerWhite = cv::Rect( face.x+round(face.width*(1.-genderValue))+1, face.y+face.height+genderPos+2, 1,  8 );
//            cv::rectangle(frame,theGender,cv::Scalar(0,0,0),CV_FILLED);
//            cv::rectangle(frame,theGenderInnerWhite,cv::Scalar(255,255,255),CV_FILLED);

            // visualize head pose
            // for this purpose, yaw and pitch are normalized in [-1...1] by HeadPose
            float yawValue = 1 - ((person.getHeadYaw()+1.)/2.);
            float pitchValue = (person.getHeadPitch()+1.)/2.;
            cv::line(frame, cv::Point(face.x+face.width/2,face.y+face.height/2), cv::Point(face.x+yawValue*face.width,face.y+pitchValue*face.height), cv::Scalar(255,255,255), 2);


#if RECORDING
            //store to csv...
            storeFile << frameCount << "," << person.getID() << "," << person.getTime() << "," << person.getAge() << "," << person.getGender() << ","
                      << person.getFaceRect().x << "," << person.getFaceRect().y << "," << person.getFaceRect().width << ","
                      << person.getHeadYaw() << "," << person.getHeadPitch() << "\n";


            if (person.getTime()-endTimePreviousBin > outputBinSizeInMiliSec)
            {
                binCounter        += 1;
                endTimePreviousBin = person.getTime();

                //store to csv...
                storeFileBinned << binCounter    << "," << uniqueIDsInOutput.size() << "," << newIDsThisBin.size() << "," << crowdsight.getPeopleCount() << "," << crowdsight.getPeopleCount()-lastBinsPeopleCounter << ","
                                << ages.at(0)    << "," << ages.at(1)    << "," << ages.at(2)    << "," << ages.at(3) << ","
                                << genders.at(0) << "," << genders.at(1) << ","
                                << yaws.at(0)    << "," << yaws.at(1)    << "," << yaws.at(2)    << ","
                                << pitches.at(0) << "," << pitches.at(1) << "," << pitches.at(2) << ","  << "\n";

                ages.at(0)    = 0;
                ages.at(1)    = 0;
                ages.at(2)    = 0;
                ages.at(3)    = 0;

                genders.at(0) = 0;
                genders.at(1) = 0;

                yaws.at(0)    = 0;
                yaws.at(1)    = 0;
                yaws.at(2)    = 0;

                pitches.at(0) = 0;
                pitches.at(1) = 0;
                pitches.at(2) = 0;

                newIDsThisBin.clear();

                lastBinsPeopleCounter = crowdsight.getPeopleCount();

            }

            int ageBin = cv::min(3,person.getAge()/agesBinSize);
            ages.at(ageBin) += 1;

            if      (person.getGender() <=0)   { genders.at(0) += 1; }
            else                               { genders.at(1) += 1; }

            if      (person.getYaw() <=0.38)   { yaws.at(0) += 1; }
            else if (person.getYaw() <=0.54)   { yaws.at(1) += 1; }
            else                               { yaws.at(2) += 1; }

            if      (person.getPitch() <=0.38) { pitches.at(0) += 1; }
            else if (person.getPitch() <=0.54) { pitches.at(1) += 1; }
            else                               { pitches.at(2) += 1; }

            std::vector<int>::iterator itor = std::find(uniqueIDsInOutput.begin(), uniqueIDsInOutput.end(), person.getID());
            if (itor == uniqueIDsInOutput.end())
            {
                uniqueIDsInOutput.push_back(person.getID());
                newIDsThisBin.push_back(person.getID());
            }
#endif
        }

#if RECORDING
        if (people.size()==0)
        {
            // when no people are recognized in a frame, store zeros so the csv file will be synchronous with the video
            storeFile << frameCount << "," << "0" << "," << "0" << "," << "0" << "," << "0" << ","
                      << "0" << "," << "0" << "," << "0" << "," << "0" << "," << "0" << "\n";
        }
#endif






        std::ostringstream peopleCounter_string;
        peopleCounter_string << "People counter: ";
        cv::putText(frame, peopleCounter_string.str(), cv::Point(3, 13),cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0,0,0),2);
        std::ostringstream peopleCounterNR_string;
        peopleCounterNR_string << crowdsight.getPeopleCount();
        cv::putText(frame, peopleCounterNR_string.str(), cv::Point(50, 40),cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0,0,0),2);
        //Show processed frame

        //    cv::Mat bigframe;
        //    cv::resize(frame,bigframe,cv::Size(1280,1024));
        //    cv::imshow(HUMAN_NAME, bigframe);

        cv::imshow(HUMAN_NAME, frame);

#if RECORDING
        if(!firstFrameToBeWritten)
          videoOutput << frame;
        else
          firstFrameToBeWritten = false;
#endif

        //    cv::waitKey();

        //Press 'q' to quit the program
        char key = cv::waitKey(1);
        if (key == 'q')
        {
#if RECORDING
            storeFile.close();
#endif
            break;
        }
    }
    return 0;
}
