#include "Models.hpp"
#include <stdexcept>


using namespace cv;
using namespace std;

namespace thymio_tracker
{

#define M_PI (std::atan(1)*4)

Vec3d toVec(Point3f _p)
{
    return Vec3d(_p.x,_p.y,_p.z);
}
Point3f toPoint(Vec3d _p)
{
    return Point3f(_p[0],_p[1],_p[2]);
}

void Object3D::draw(Mat& img, const Mat& cameraMatrix, const Mat& distCoeffs, const Affine3d& poseCam) const
{
    //for(unsigned int e=0;e<mEdges.size();e++)
    //    drawEdge(mEdges[e], img, cameraMatrix, distCoeffs, poseCam);
    
    for(unsigned int e=0;e<mEdgesTrack.size();e++)
        drawEdgeTrack(mEdgesTrack[e], img, cameraMatrix, distCoeffs, poseCam);
    
    for(unsigned int v=0;v<mVertices.size();v++)
        drawVertice(mVertices[v], img, cameraMatrix, distCoeffs, poseCam);

    for(unsigned int v=0;v<mPlanarSurfaces.size();v++)
        drawSurface(mPlanarSurfaces[v], img, cameraMatrix, distCoeffs, poseCam);
    
    //draw object frame (axis XYZ)
    vector<Point3f> framePoints;
    framePoints.push_back(Point3f(0,0,0));
    framePoints.push_back(Point3f(0.03,0,0));
    framePoints.push_back(Point3f(0,0.03,0));
    framePoints.push_back(Point3f(0,0,0.03));

    for(int p=0;p<4;p++)framePoints[p]=pose*framePoints[p];
    vector<Point2f> vprojVertices;
    projectPoints(framePoints, poseCam.rvec(), poseCam.translation(), cameraMatrix, distCoeffs, vprojVertices);
    line(img, vprojVertices[0], vprojVertices[1], Scalar(0,0,255), 2);
    line(img, vprojVertices[0], vprojVertices[2], Scalar(0,255,0), 2);
    line(img, vprojVertices[0], vprojVertices[3], Scalar(255,0,0), 2);

}

void Object3D::drawVertice(const Point3f &_vertice, Mat &img, const Mat &cameraMatrix, const Mat &distCoeffs, const Affine3d &poseCam) const
{
    //transform using object pose
    vector<Point3f> pobj; pobj.push_back(pose*_vertice);
    Point3f pcam; pcam= poseCam * pose * pobj[0];
    
    //project
    vector<Point2f> vprojVertices;
    projectPoints(pobj, poseCam.rvec(), poseCam.translation(), cameraMatrix, distCoeffs, vprojVertices);
    

    if(pcam.z>0 && 1./pcam.z > 0)
        circle(img, vprojVertices[0], 3./pcam.z, Scalar(255,255,0),2);
}

void Object3D::drawEdge(const ModelEdge &_edge, const Mat &img, const Mat &cameraMatrix, const Mat &distCoeffs, const Affine3d &poseCam) const
{
    vector<Point3f> LineObj; LineObj.push_back(pose*_edge.ptFrom);LineObj.push_back(pose*_edge.ptTo);
    vector<Point3f> LineCam; LineCam.push_back(poseCam*LineObj[0]);LineCam.push_back(poseCam*LineObj[1]);
    
    vector<Point2f> vprojVertices;
    projectPoints(LineObj, poseCam.rvec(), poseCam.translation(), cameraMatrix, distCoeffs, vprojVertices);
    //if(LineCam[0].z>0 && LineCam[1].z>0)
        line(img, vprojVertices[0], vprojVertices[1], Scalar(0,0,255), 2);   

}
void Object3D::drawSurface(const planarSurface &_edge, const Mat &img, const Mat &cameraMatrix, const Mat &distCoeffs, const Affine3d &poseCam) const
{
    vector<Point3f> ObjPoints;
    ObjPoints.push_back(_edge.center);
    ObjPoints.push_back(_edge.center+toPoint(0.01*_edge.normal));
    ObjPoints.push_back(_edge.center+toPoint(_edge.radius1*_edge.b1+_edge.radius2*_edge.b2));
    ObjPoints.push_back(_edge.center+toPoint(_edge.radius1*_edge.b1-_edge.radius2*_edge.b2));
    ObjPoints.push_back(_edge.center+toPoint(_edge.radius1*-_edge.b1-_edge.radius2*_edge.b2));
    ObjPoints.push_back(_edge.center+toPoint(_edge.radius1*-_edge.b1+_edge.radius2*_edge.b2));

    vector<Point3f> LineObj;
    for(uint i=0;i<ObjPoints.size();i++) 
        LineObj.push_back(pose*ObjPoints[i]);

    vector<Point3f> LineCam;
    for(uint i=0;i<LineObj.size();i++) 
        LineCam.push_back(poseCam*LineObj[i]);
    
    vector<Point2f> vprojVertices;
    projectPoints(LineObj, poseCam.rvec(), poseCam.translation(), cameraMatrix, distCoeffs, vprojVertices);

    Point3f ptLine = poseCam * pose * _edge.center;
    Vec3d ray = Vec3d(ptLine.x,ptLine.y,ptLine.z);
    ray = ray / norm(ray);
    Vec3d normal_cam = poseCam.rotation() * pose.rotation() * _edge.normal;
    float view = -normal_cam.dot(ray);

    //if(LineCam[0].z>0 && LineCam[0].z - LineCam[1].z >0)
    if(view > 0)
    {
        //std::cout<<"surf viewScore"<<view<<std::endl;
        //print normal
        line(img, vprojVertices[0], vprojVertices[1], Scalar(0,255,255), 2); 

        //print surface
        for(int i=0;i<4;i++)
            line(img, vprojVertices[2+i], vprojVertices[2+((i+1) % 4)], Scalar(255,0,255), 2);   
    }
       

}


void Object3D::drawEdgeTrack(const ModelEdgeTrack &_edge, const Mat &img, const Mat &cameraMatrix, const Mat &distCoeffs, const Affine3d &poseCam) const
{
    //check visibility
    Vec3d normal1_cam = poseCam.rotation() * pose.rotation() * _edge.normal1;
    Vec3d normal2_cam = poseCam.rotation() * pose.rotation() * _edge.normal2;

    //get angle between normals and ray
    Point3f ptLine = poseCam * pose * _edge.ptFrom;
    Vec3d ray = Vec3d(ptLine.x,ptLine.y,ptLine.z);
    ray = ray / norm(ray);
    float angleView1 = acos(normal1_cam.dot(ray));
    float angleView2 = acos(normal2_cam.dot(ray));

    vector<Point3f> LineObj; LineObj.push_back(pose*_edge.ptFrom);LineObj.push_back(pose*_edge.ptTo);
    
    vector<Point2f> vprojVertices;
    projectPoints(LineObj, poseCam.rvec(), poseCam.translation(), cameraMatrix, distCoeffs, vprojVertices);

    //edge is part of silouette if one face is visible and the other not.
    if( (angleView1 > M_PI/2 && angleView2 < M_PI/2) || (angleView1 < M_PI/2 && angleView2 > M_PI/2))
        line(img, vprojVertices[0], vprojVertices[1], Scalar(0,0,255), 2);   


    //print normals
    vector<Point3f> LineNormals;
    LineNormals.push_back((_edge.ptFrom + _edge.ptTo)/2.);
    LineNormals.push_back((_edge.ptFrom + _edge.ptTo)/2. + toPoint(0.01*_edge.normal1));
    LineNormals.push_back((_edge.ptFrom + _edge.ptTo)/2. + toPoint(0.01*_edge.normal2));

    vector<Point3f> LineNormals_p;
    for(int i=0;i<3;i++)
        LineNormals_p.push_back(pose * LineNormals[i]);

    vector<Point2f> vprojNormals;
    projectPoints(LineNormals_p, poseCam.rvec(), poseCam.translation(), cameraMatrix, distCoeffs, vprojNormals);
    line(img, vprojNormals[0], vprojNormals[1], Scalar(0,255,0), 1);  
    line(img, vprojNormals[0], vprojNormals[2], Scalar(255,0,0), 1);  
    
}

float rotationVSfrontoparallel(const Vec3d &rvec)
{
    Affine3d cameraRotPose = Affine3d().rotate(rvec);
    Point3f frontoPerp(0.,0,-1);
    
    //express frontoPerp in cam
    Point3f frontoPerpCam=cameraRotPose*frontoPerp;
    //get angle with frontoperp
    return acos(frontoPerpCam.z);
}

bool compareByDiscriminativePower(const DetectionGH &a, const DetectionGH &b)
{
    return a.discriminativePower > b.discriminativePower;
}

//get all subsets of p elements out of a n elements set
bool getNextSetPointers(unsigned int *pointers, int n, int p)
{
    int nbPtStuckAtEnd=0;
    for(int i=p-1;i>=0;i--)
        if((int)pointers[i]==n-p+i)
            nbPtStuckAtEnd++;
    
    if(nbPtStuckAtEnd==0)//if none stuck then can simply move last one
        pointers[p-1]++;
    else if(nbPtStuckAtEnd==p)//all of them are stuck and we didn t find anything good=> lost
    {
        //std::cout<< "ratio_max = "<<ratio_max<<std::endl;
        return false;
    }
    else//some of them are at the end
    {
        //need to move the (nbBasePnp-1-nbPtStuckAtEnd)th pointer right and set the others to follow
        pointers[p-1-nbPtStuckAtEnd]++;
        for(int i=p-1-nbPtStuckAtEnd+1;i<p;i++)
            pointers[i]=pointers[i-1]+1;
    }
    return true;
}

bool Object3D::getPose(const IntrinsicCalibration &_mCalib, vector<DetectionGH> mMatches, Affine3d &robotPose, bool init) const
{
    //if don t have 4 measures then problem not solvable
    if(mMatches.size() < 4)
        return false;
    
    //sort matches with respect to discriminative power to ease ransac later
    //put higher discriminative detections first
    std::sort(mMatches.begin(), mMatches.end(), compareByDiscriminativePower);
    
    //create list of 3d points corresponding to detected projections
    vector<Point3f> detectedVertices;
    for(unsigned int i=0;i<mMatches.size();i++)detectedVertices.push_back(mVertices[mMatches[i].id]);

    //use opencv function
    //get previous position if there is any
    Vec3d rvec_prec,tvec_prec;
    if(init)
    {
        rvec_prec=robotPose.rvec();
        tvec_prec=robotPose.translation();
    }
    else
    {
        rvec_prec = Vec3d(0.,M_PI,0.);
        tvec_prec= Vec3d(0.,0.,0.4);
    }
    
    //do a kind of ransac: try different subset to compute pose util find that more than majority agrees
    //if not consider tracker lost
    const unsigned int nbBasePnp=4;//take four points out of set
    unsigned int pointers[nbBasePnp];
    for(unsigned int i=0;i<nbBasePnp;i++)pointers[i]=i;//set first pointers as first elements of list

    //limit the number of trials:
    int cpt_trials = 0;
    int nb_trials_max = 20;
   
    while(cpt_trials<nb_trials_max)
    {
        cpt_trials ++;
        //want to test all the unsorted subsets of 4 points
        Vec3d rvec,tvec;
        //if(!init)    {rvec=rvec_prec;tvec=tvec_prec;}
        rvec=rvec_prec;tvec=tvec_prec;
        
        //create vectors corresponding to subset
        vector<Point3f> subsetVertices;
        vector<Point2f> subsetProjections;
        for(unsigned int i=0;i<nbBasePnp;i++)
        {
            subsetVertices.push_back(detectedVertices[pointers[i]]);
            subsetProjections.push_back(mMatches[pointers[i]].position);
        }
        
        
        //compute pose with subset
        cv::solvePnP(subsetVertices, subsetProjections, _mCalib.cameraMatrix, _mCalib.distCoeffs, rvec, tvec, !init);
        //cv::solvePnPRansac(detectedVertices, pointPositions, cameraMatrix, distCoeffs, rvec, tvec);
        
        //check if estimated transformation is possible (if rotation with respect to image plan > max rotation GH training
        //then would not be able to do this association using GH)
        //std::cout<< "rotationVSfrontoparallel(rvec) = "<<rotationVSfrontoparallel(rvec)<<std::endl;
        if(rotationVSfrontoparallel(rvec)<M_PI/4)
        {
            
            //check how many points agree
            float threshold_proj=2.;//set error max to 2 pixels
            //float threshold_proj=40.;//set error max to 5 pixels
            unsigned int nbPointAgree=0;
            vector<Point2f> vProjPoints;//project all points
            projectPoints(detectedVertices, rvec, tvec, _mCalib.cameraMatrix, _mCalib.distCoeffs, vProjPoints);
            for(unsigned int i=0;i<mMatches.size();i++)
                if(norm(vProjPoints[i]-mMatches[i].position)<threshold_proj)
                    nbPointAgree++;
            
            //if more than majority agrees then fine, recompute objects pose with all points which agree and return it
            //might be better to instead do a tuckey optimisation with all points, as their might be points rejected with
            //first subset which after some refining would be back in...
            //can also run several iterations of the previous selection and following optimisation
            if(nbPointAgree>mMatches.size()*0.75)
            //if(nbPointAgree>mMatches.size()/6)
            {
                vector<Point3f> newSubsetVertices;
                vector<Point2f> newSubsetProjections;
                for(unsigned int i=0;i<mMatches.size();i++)
                    if(norm(vProjPoints[i]-mMatches[i].position)<threshold_proj)
                    {
                        newSubsetVertices.push_back(detectedVertices[i]);
                        newSubsetProjections.push_back(mMatches[i].position);
                    }
                
                cv::solvePnP(newSubsetVertices, newSubsetProjections, _mCalib.cameraMatrix, _mCalib.distCoeffs, rvec, tvec, !init);
                robotPose=Affine3d(rvec,tvec);
                //std::cout<< "ratio = "<<(float)nbPointAgree/mMatches.size()<<std::endl;
                //std::cout<< "rotationVSfrontoparallel(rvec) = "<<rotationVSfrontoparallel(rvec)<<std::endl;
                return true;
            }
        }
        
        //get next subset
        bool nextSetAvailable = getNextSetPointers(&pointers[0],mMatches.size(),nbBasePnp);

        if(!nextSetAvailable)
            return false;

        
    }
    return false;

    
}


Camera3dModel::Camera3dModel()
{
    vector<Point3f> mVerticesTemp;
    float scale=0.04;//make it about 5cm big
    float w=scale*0.4,h=scale*0.3,z=scale*0.8;
    
    //create vertices from dimensions
    mVerticesTemp.push_back(Point3f(0,0,0));
    mVerticesTemp.push_back(Point3f(w,h,z));
    mVerticesTemp.push_back(Point3f(w,-h,z));
    mVerticesTemp.push_back(Point3f(-w,-h,z));
    mVerticesTemp.push_back(Point3f(-w,h,z));
    
    //from center to image plane
    mEdges.push_back(ModelEdge(mVerticesTemp[0],mVerticesTemp[1]));
    mEdges.push_back(ModelEdge(mVerticesTemp[0],mVerticesTemp[2]));
    mEdges.push_back(ModelEdge(mVerticesTemp[0],mVerticesTemp[3]));
    mEdges.push_back(ModelEdge(mVerticesTemp[0],mVerticesTemp[4]));
    
    //image plane
    mEdges.push_back(ModelEdge(mVerticesTemp[1],mVerticesTemp[2]));
    mEdges.push_back(ModelEdge(mVerticesTemp[2],mVerticesTemp[3]));
    mEdges.push_back(ModelEdge(mVerticesTemp[3],mVerticesTemp[4]));
    mEdges.push_back(ModelEdge(mVerticesTemp[4],mVerticesTemp[1]));
}

void ThymioBlobModel::setBlobModel()
{
    //top of the robot
    //model definition:
    // 10             0
    // 11  7       4  1
    //
    // 12  8       5  2
    // 13  9       6  3
    
    //set everything in centimeters and will divide later to convert in meters
    mVertices.push_back(Point3f(0.044,0.036,0.0305));
    mVertices.push_back(Point3f(0.044,0.028,0.0305));
    mVertices.push_back(Point3f(0.044,-0.012,0.0305));
    mVertices.push_back(Point3f(0.044,-0.02,0.0305));

    mVertices.push_back(Point3f(0.036,0.028,0.0305));
    mVertices.push_back(Point3f(0.036,-0.012,0.0305));
    mVertices.push_back(Point3f(0.036,-0.02,0.0305));

    mVertices.push_back(Point3f(-0.036,0.028,0.0305));
    mVertices.push_back(Point3f(-0.036,-0.012,0.0305));
    mVertices.push_back(Point3f(-0.036,-0.02,0.0305));

    mVertices.push_back(Point3f(-0.044,0.036,0.0305));
    mVertices.push_back(Point3f(-0.044,0.028,0.0305));
    mVertices.push_back(Point3f(-0.044,-0.012,0.0305));
    mVertices.push_back(Point3f(-0.044,-0.02,0.0305));
    
    //define groups in clockwise order
    mGroup3s.push_back(ModelTriplet(0,1,4));
    mGroup3s.push_back(ModelTriplet(10,7,11));
    
    mGroup4s.push_back(ModelQuadruplet(5,2,3,6));
    mGroup4s.push_back(ModelQuadruplet(12,8,9,13));
}
void ThymioBlobModel::setEdgePlotModel()
{
    //if want to display edges
    vector<Point3f> mVerticesTop;
    mVerticesTop.push_back(Point3f(-0.0505,0.0575,0.0305));
    mVerticesTop.push_back(Point3f(-0.0505,-0.0265,0.0305));
    mVerticesTop.push_back(Point3f(0.051,-0.0265,0.0305));
    mVerticesTop.push_back(Point3f(0.051,0.0575,0.0305));
    mVerticesTop.push_back(Point3f(0.026,0.074,0.0305));
    mVerticesTop.push_back(Point3f(0.006,0.077,0.0305));
    mVerticesTop.push_back(Point3f(-0.019,0.074,0.0305));
    
    //from center to image plane
    for(unsigned int v=0;v<mVerticesTop.size();v++)
        mEdges.push_back(ModelEdge(mVerticesTop[v],mVerticesTop[(v+1)%mVerticesTop.size()]));
    
    vector<Point3f> mVerticesBottom;
    mVerticesBottom.push_back(Point3f(-0.0505,0.0575,-0.0135));
    mVerticesBottom.push_back(Point3f(-0.0505,-0.0265,-0.0135));
    mVerticesBottom.push_back(Point3f(0.051,-0.0265,-0.0135));
    mVerticesBottom.push_back(Point3f(0.051,0.0575,-0.0135));
    mVerticesBottom.push_back(Point3f(0.026,0.074,-0.0135));
    mVerticesBottom.push_back(Point3f(0.006,0.077,-0.0135));
    mVerticesBottom.push_back(Point3f(-0.019,0.074,-0.0135));
  
    //from center to image plane
    for(unsigned int v=0;v<mVerticesBottom.size();v++)
        mEdges.push_back(ModelEdge(mVerticesBottom[v],mVerticesBottom[(v+1)%mVerticesBottom.size()]));
    
    //vertical edges
    vector<Point3f> mVerticesVertical;
    //create vertices from dimensions
    mVerticesVertical.push_back(Point3f(-0.0505,0.0575,0.0305));
    mVerticesVertical.push_back(Point3f(-0.0505,0.0575,-0.0135));
    mVerticesVertical.push_back(Point3f(-0.0505,-0.0265,0.0305));
    mVerticesVertical.push_back(Point3f(-0.0505,-0.0265,-0.0135));
    mVerticesVertical.push_back(Point3f(0.051,-0.0265,0.0305));
    mVerticesVertical.push_back(Point3f(0.051,-0.0265,-0.0135));
    mVerticesVertical.push_back(Point3f(0.051,0.0575,0.0305));
    mVerticesVertical.push_back(Point3f(0.051,0.0575,-0.0135));
    
    //from center to image plane
    for(unsigned int v=0;v<mVerticesVertical.size()/2;v++)
        mEdges.push_back(ModelEdge(mVerticesVertical[2*v],mVerticesVertical[2*v+1]));
}
void ThymioBlobModel::setEdgeTrackModel()
{
    Point3f heightRobot(0.,0.,0.0445);

    //define edges for tracking
    vector<Point3f> mVerticesTrackTop;
    mVerticesTrackTop.push_back(Point3f(-0.055,0.055,0.0305));
    mVerticesTrackTop.push_back(Point3f(-0.055,-0.0295,0.0305));
    mVerticesTrackTop.push_back(Point3f(0.055,-0.0295,0.0305));
    mVerticesTrackTop.push_back(Point3f(0.055,0.055,0.0305));

    float sqrt2_2 = sqrt(2.)/2.;
    //bissectrice of edge
    Vec3d mNormals1TrackTop(0.,0.,-1.);
    
    vector<Vec3d> mNormals2TrackTop;
    mNormals2TrackTop.push_back(Vec3d(sqrt2_2,0,0));
    mNormals2TrackTop.push_back(Vec3d(0.,sqrt2_2,0));
    mNormals2TrackTop.push_back(Vec3d(-sqrt2_2,0,0));

    for(unsigned int v=0;v<mVerticesTrackTop.size()-1;v++)
        //mEdgesTrack.push_back(ModelEdgeTrack(mVerticesTrackTop[v],mVerticesTrackTop[(v+1)],mNormalsTrackTop[v],mEdgesRounding[v]));
        mEdgesTrack.push_back(ModelEdgeTrack(mVerticesTrackTop[v],mVerticesTrackTop[(v+1)],mNormals1TrackTop,mNormals2TrackTop[v]));


    vector<Point3f> mVerticesTrackTopRound;
    vector<Vec3d> mNormals2TrackTopRound;

    //round bit
    int nbRoundCut = 4;
    //more elliptic than perfect circle => define two radii
    float radiusFront = 0.08;
    float radiusSides = 0.078;
    for(int i=-nbRoundCut;i<=nbRoundCut;i++)
    {
        float angle = - i * M_PI / (4. * nbRoundCut);
        float radius = radiusSides * abs(i) / nbRoundCut + radiusFront * (nbRoundCut - abs(i)) / nbRoundCut;
        mVerticesTrackTopRound.push_back(Point3f(radius * sin(angle),radius * cos(angle),0.0305));    
        float angleSeg = - ((float)i+0.5) * M_PI / (4. * nbRoundCut);
        mNormals2TrackTopRound.push_back(Vec3d(- sin(angleSeg),- cos(angleSeg),0.));    
    }

    for(unsigned int v=0;v<mVerticesTrackTopRound.size()-1;v++)
        mEdgesTrack.push_back(ModelEdgeTrack(mVerticesTrackTopRound[v],mVerticesTrackTopRound[(v+1)],mNormals1TrackTop,mNormals2TrackTopRound[v]));

    //edges from bottom
    Vec3d mNormals1TrackBottom(0.,0.,1.);
    int nbEdgesTop = mEdgesTrack.size();
    for(int i=0; i< nbEdgesTop;i++)
        mEdgesTrack.push_back(ModelEdgeTrack(mEdgesTrack[i].ptFrom - heightRobot,mEdgesTrack[i].ptTo - heightRobot,mNormals1TrackBottom,mEdgesTrack[i].normal2));
    

    //vertical edges
    mEdgesTrack.push_back(ModelEdgeTrack(mVerticesTrackTop[0],mVerticesTrackTop[0] - heightRobot,Vec3d(sqrt2_2,-sqrt2_2,0.),Vec3d(1.,0.,0.)) );
    mEdgesTrack.push_back(ModelEdgeTrack(mVerticesTrackTop[1],mVerticesTrackTop[1] - heightRobot,Vec3d(0.,1.,0.),Vec3d(1.,0.,0.)) );
    mEdgesTrack.push_back(ModelEdgeTrack(mVerticesTrackTop[2],mVerticesTrackTop[2] - heightRobot,Vec3d(0.,1.,0.),Vec3d(-1.,0.,0.)) );
    mEdgesTrack.push_back(ModelEdgeTrack(mVerticesTrackTop[3],mVerticesTrackTop[3] - heightRobot,Vec3d(-sqrt2_2,-sqrt2_2,0.),Vec3d(-1.,0.,0.)) );

}
    


ThymioBlobModel::ThymioBlobModel()
{
    setBlobModel();
    setEdgePlotModel();
    setEdgeTrackModel();
    setSurfacesModel();
}

void ThymioBlobModel::setSurfacesModel()
{
    mPlanarSurfaces.reserve(15);
    //wheels
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f(0.055, 0.015,0.005), cv::Vec3d(0.,1.,0.), cv::Vec3d(0.,0.,1.), 0.01,0.015));
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f(0.055,-0.015,0.005), cv::Vec3d(0.,1.,0.), cv::Vec3d(0.,0.,1.), 0.01,0.015));
    //top blobs
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f( 0.04, -0.015,0.031), cv::Vec3d(1.,0.,0.), cv::Vec3d(0.,1.,0.), 0.01));
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f( 0.04,  0.033,0.031), cv::Vec3d(1.,0.,0.), cv::Vec3d(0.,1.,0.), 0.01));
    //back rectangle
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f( 0.03, -0.0295,0.012), cv::Vec3d(1.,0.,0.), cv::Vec3d(0.,0.,1.), 0.01));
    
    //front rectanglular bits
    int nbRoundCut = 2;
    //more elliptic than perfect circle => define two radii
    float radiusFront = 0.08;
    float radiusSides = 0.078;
    for(int i=-nbRoundCut;i<=0;i++)
    {
        float angle = - i * M_PI / (4.7 * nbRoundCut);
        float radius = radiusSides * abs(i) / nbRoundCut + radiusFront * (nbRoundCut - abs(i)) / nbRoundCut;
        Point3f center(radius * sin(angle),radius * cos(angle),0.013);    
        mPlanarSurfaces.push_back(planarSurface(center,cv::Vec3d(-cos(angle),sin(angle),0.),cv::Vec3d(0,0,1.),0.01,0.005));    
    }

    //create symetrical surfaces
    for(int i=0;i<7;i++)
    {
        planarSurface newSurf;
        newSurf.setSymetry(mPlanarSurfaces[i]);
        mPlanarSurfaces.push_back(newSurf);
    }

    /*mPlanarSurfaces.reserve(27);
    //wheels
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f(0.055, 0.015,0.005), cv::Vec3d(0.,1.,0.), cv::Vec3d(0.,0.,1.), 0.01,0.015));
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f(0.055,-0.015,0.005), cv::Vec3d(0.,1.,0.), cv::Vec3d(0.,0.,1.), 0.01,0.015));
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f(0.055,0.0,0.015), cv::Vec3d(0.,1.,0.), cv::Vec3d(0.,0.,1.), 0.015,0.01));

    //crosses on side
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f(0.055,0.033,0.01), cv::Vec3d(0.,1.,0.), cv::Vec3d(0.,0.,1.), 0.015,0.015));


    //top blobs
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f( 0.04, -0.015,0.031), cv::Vec3d(1.,0.,0.), cv::Vec3d(0.,1.,0.), 0.01));
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f( 0.035, -0.015,0.031), cv::Vec3d(1.,0.,0.), cv::Vec3d(0.,1.,0.), 0.01));
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f( 0.04, -0.01,0.031), cv::Vec3d(1.,0.,0.), cv::Vec3d(0.,1.,0.), 0.01));
    
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f( 0.04,  0.033,0.031), cv::Vec3d(1.,0.,0.), cv::Vec3d(0.,1.,0.), 0.01));
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f( 0.035,  0.033,0.031), cv::Vec3d(1.,0.,0.), cv::Vec3d(0.,1.,0.), 0.01));
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f( 0.04,  0.028,0.031), cv::Vec3d(1.,0.,0.), cv::Vec3d(0.,1.,0.), 0.01));
    
    //back rectangle
    mPlanarSurfaces.push_back(planarSurface(cv::Point3f( 0.03, -0.0295,0.012), cv::Vec3d(1.,0.,0.), cv::Vec3d(0.,0.,1.), 0.01));
    
    //front rectanglular bits
    int nbRoundCut = 2;
    //more elliptic than perfect circle => define two radii
    float radiusFront = 0.08;
    float radiusSides = 0.078;
    for(int i=-nbRoundCut;i<=0;i++)
    {
        float angle = - i * M_PI / (4.7 * nbRoundCut);
        float radius = radiusSides * abs(i) / nbRoundCut + radiusFront * (nbRoundCut - abs(i)) / nbRoundCut;
        Point3f center(radius * sin(angle),radius * cos(angle),0.013);    
        mPlanarSurfaces.push_back(planarSurface(center,cv::Vec3d(-cos(angle),sin(angle),0.),cv::Vec3d(0,0,1.),0.01,0.005));    
    }

    //create symetrical surfaces (don't forget last surface is at center => don't want symetrical copy)
    for(int i=0;i<13;i++)
    {
        planarSurface newSurf;
        newSurf.setSymetry(mPlanarSurfaces[i]);
        mPlanarSurfaces.push_back(newSurf);
    }
    */


}

void Object3D::allocateSurfaceLearning()
{
    //allocate, but not the symetrical copies ...
    for(unsigned int v=0;v<mPlanarSurfaces.size();v++)
        if(!mPlanarSurfaces[v].isSymetricCopy)
            mPlanarSurfaces[v].allocateLearning();
}

void Object3D::learnAppearance(cv::Mat &img, const IntrinsicCalibration &_mCalib, const cv::Affine3d& poseCam)
{
    for(unsigned int v=0;v<mPlanarSurfaces.size();v++)
    {
        planarSurface &surf = mPlanarSurfaces[v];

        //project surface corners to check if visible and get homography from image to surface model
        Point3f ptLine = poseCam * pose * surf.center;
        Vec3d ray = Vec3d(ptLine.x,ptLine.y,ptLine.z);        ray = ray / norm(ray);
        Vec3d normal_cam = poseCam.rotation() * pose.rotation() * surf.normal;
        //float viewScore = -normal_cam.dot(ray);
        float viewScore = 1.-2.*acos(-normal_cam.dot(ray))/3.141592;

        if(viewScore > 0)
        {
            vector<Point3f> ObjPoints;
            ObjPoints.push_back(surf.center+toPoint(surf.radius1*surf.b1+surf.radius2*surf.b2));
            ObjPoints.push_back(surf.center+toPoint(surf.radius1*surf.b1-surf.radius2*surf.b2));
            ObjPoints.push_back(surf.center+toPoint(surf.radius1*-surf.b1-surf.radius2*surf.b2));
            ObjPoints.push_back(surf.center+toPoint(surf.radius1*-surf.b1+surf.radius2*surf.b2));

            vector<Point3f> LineObj;
            for(uint i=0;i<ObjPoints.size();i++) 
                LineObj.push_back(pose*ObjPoints[i]);
            
            vector<Point2f> vprojVertices;
            projectPoints(LineObj, poseCam.rvec(), poseCam.translation(), _mCalib.cameraMatrix, _mCalib.distCoeffs, vprojVertices);

            //get homography
            vector<Point2f> modelPoints;
            modelPoints.push_back(Point2f(surf.mImagePtr->size().width,0));
            modelPoints.push_back(Point2f(surf.mImagePtr->size().width,surf.mImagePtr->size().height));
            modelPoints.push_back(Point2f(0,surf.mImagePtr->size().height));
            modelPoints.push_back(Point2f(0,0));

            cv::Mat homography = cv::findHomography(vprojVertices, modelPoints);

            //warp current image in it
            cv::Mat patchCurr( surf.mImagePtr->size().height, surf.mImagePtr->size().width, img.type() );
            cv::warpPerspective( img, patchCurr, homography, patchCurr.size() );

            //accumulate
            for(int i=0;i<surf.mImagePtr->size().height;i++)
                for(int j=0;j<surf.mImagePtr->size().width;j++)
                    //surf.mImage.at<float>(i,j) += viewScore* (float)patchCurr.at<unsigned char>(i,j);
                    surf.mImagePtr->at<float>(i,j) += viewScore* (float)patchCurr.at<unsigned char>(i,j);

            *surf.weight += viewScore;

        }

    }

}

void Object3D::writeSurfaceLearned(const std::string& robotFilePath)
{

    //convert mImage which is in float to unsigned char as it will be used during tracking
    for(unsigned int v=0;v<mPlanarSurfaces.size();v++)
    {
        planarSurface &surf = mPlanarSurfaces[v];
        if(!surf.isSymetricCopy)
        {
            cv::Mat buff( surf.mImage.size().height, surf.mImage.size().width, CV_8UC1);
            for(int i=0;i<surf.mImage.size().height;i++)
                for(int j=0;j<surf.mImage.size().width;j++)
                    buff.at<unsigned char>(i,j) = surf.mImage.at<float>(i,j)/(*surf.weight);

            surf.mImage.release();
            buff.copyTo(surf.mImage);
        }
    }

    //save to xml file
    std::vector<cv::Mat> imageVector;
    for(unsigned int v=0;v<mPlanarSurfaces.size();v++)
        if(!mPlanarSurfaces[v].isSymetricCopy)
            imageVector.push_back(mPlanarSurfaces[v].mImage);


    cv::FileStorage store(robotFilePath, cv::FileStorage::WRITE);
    cv::write(store,"imageVector",imageVector);
    store.release();
}

void Object3D::readSurfaceLearned(cv::FileStorage& robotModelStorage)
{
    std::vector<cv::Mat> imageVectorr;
    cv::FileNode n1 = robotModelStorage["imageVector"];
    cv::read(n1,imageVectorr);
    robotModelStorage.release();

    for(unsigned int v=0;v<mPlanarSurfaces.size();v++)
        if(!mPlanarSurfaces[v].isSymetricCopy)
            imageVectorr[v].copyTo(mPlanarSurfaces[v].mImage);
    
}

struct surfaceMatch {
    cv::Point3f objectPoints;
    cv::Point2f imagePoints;
    float score;
} ;

    

bool compareByScore(const surfaceMatch &a, const surfaceMatch &b)
{
    return a.score > b.score;
}

bool Object3D::track(const cv::Mat &img, const cv::Mat &prev_img, const IntrinsicCalibration &_mCalib, const cv::Affine3d& prevPoseCam, cv::Affine3d& poseCam) const
{
    //project textured planar surfaces to current image using the previous pose
    //do NCC search to find displacement up to drift for each surface using frame to frame similarity
    //refine with MI search and parabolic fitting to correct drift then PnP
    //to retrieve the 3D pose from the sets of 2D matches

    int window_search_size = 32;
    int half_window_size = window_search_size/2;

    int window_search_size_drift = 6;
    int half_window_size_drift = window_search_size_drift/2;

    std::vector<surfaceMatch> mSurfaceMatches;

    for(unsigned int v=0;v<mPlanarSurfaces.size();v++)
    {
        const planarSurface &surf = mPlanarSurfaces[v];

        //project surface corners to check if visible and get homography from image to surface model
        Point3f ptLine = prevPoseCam * pose * surf.center;
        Vec3d ray = Vec3d(ptLine.x,ptLine.y,ptLine.z);        ray = ray / norm(ray);
        Vec3d normal_cam = prevPoseCam.rotation() * pose.rotation() * surf.normal;
        float viewScore = 1.-2.*acos(-normal_cam.dot(ray))/3.141592;

        if(viewScore > 0.2)
        {
            vector<Point3f> ObjPoints;
            ObjPoints.push_back(surf.center+toPoint(surf.radius1*surf.b1+surf.radius2*surf.b2));
            ObjPoints.push_back(surf.center+toPoint(surf.radius1*surf.b1-surf.radius2*surf.b2));
            ObjPoints.push_back(surf.center+toPoint(-surf.radius1*surf.b1-surf.radius2*surf.b2));
            ObjPoints.push_back(surf.center+toPoint(-surf.radius1*surf.b1+surf.radius2*surf.b2));
            ObjPoints.push_back(surf.center);

            vector<Point3f> LineObj;
            for(uint i=0;i<ObjPoints.size();i++) 
                LineObj.push_back(pose*ObjPoints[i]);
            
            //get position of surface corners in previous image
            vector<Point2f> vprojVertices;
            projectPoints(LineObj, prevPoseCam.rvec(), prevPoseCam.translation(), _mCalib.cameraMatrix, _mCalib.distCoeffs, vprojVertices);
            
            //get the minimal support rectangle which contains the projected surface
            Rect box = cv::boundingRect(cv::Mat(vprojVertices));

            //if reasonable size and entirely projects in image
            if(box.size().width<100 && box.size().height<100)
            if(box.x >=0 && box.y >=0 && box.x + box.size().width < img.size().width && box.y + box.size().height < img.size().height)
            {

                 //get projected corners coordinates in the bounding box
                vector<Point> vprojVertices_bb;
                for(uint v=0;v<vprojVertices.size();v++)
                    vprojVertices_bb.push_back(vprojVertices[v]-Point2f(box.x, box.y));

                vector<Point2f> vprojVertices_bbF;
                for(uint v=0;v<vprojVertices.size();v++)
                    vprojVertices_bbF.push_back(vprojVertices[v]-Point2f(box.x, box.y));


                //create the corresponding mask in box wheer surface is defined
                cv::Mat mask = cv::Mat::zeros( box.size(), CV_8UC1 );
                cv::fillConvexPoly(mask, &vprojVertices_bb[0], 4, 1);//4: use only 4 first corners in vertices to define contour, 5th is center

                //use previous image to define what we are searching for
                cv::Mat patchCurr(box.size(), CV_8UC1);
                prev_img(box).copyTo(patchCurr);

                //compute NCC over search window
                //get the roi of the current image on which to compute similarity with patch
                int myRoi_l = box.x-half_window_size; myRoi_l = (myRoi_l<0)?0:myRoi_l;
                int myRoi_t = box.y-half_window_size; myRoi_t = (myRoi_t<0)?0:myRoi_t;
                int myRoi_r = box.x+box.size().width+half_window_size; myRoi_r = (myRoi_r>img.size().width)?img.size().width:myRoi_r;
                int myRoi_d = box.y+box.size().height+half_window_size; myRoi_d = (myRoi_d>img.size().height)?img.size().height:myRoi_d;       

                cv::Rect myROI(myRoi_l,myRoi_t,myRoi_r-myRoi_l,myRoi_d-myRoi_t);//region of interest is around current position of point
                
                //verify that the search region is valid
                int result_cols = myROI.size().width - box.size().width + 1;
                int result_rows = myROI.size().height - box.size().height + 1;

                if(result_cols > half_window_size && result_rows > half_window_size)
                {
                    cv::Mat resultNCC = cv::Mat::zeros( result_rows, result_cols, CV_32FC1 );
                    cv::matchTemplate( img(myROI), patchCurr, resultNCC, CV_TM_SQDIFF, mask);
                    
                    /// Localizing the best match with minMaxLoc searching for max NCC
                    double minVal; double maxVal; cv::Point minLoc; cv::Point maxLoc;
                    cv::minMaxLoc( resultNCC, &minVal, &maxVal, &minLoc, &maxLoc, cv::Mat() );

                    //now we have hopefully the position of the surface up to some drift
                    //so we want to use the model to correct the drift

                    //get homography from surface model to current image ROI
                    vector<Point2f> modelPoints;
                    modelPoints.push_back(Point2f(surf.mImagePtr->size().width,0));
                    modelPoints.push_back(Point2f(surf.mImagePtr->size().width,surf.mImagePtr->size().height));
                    modelPoints.push_back(Point2f(0,surf.mImagePtr->size().height));
                    modelPoints.push_back(Point2f(0,0));
                    modelPoints.push_back(Point2f(surf.mImagePtr->size().width/2,surf.mImagePtr->size().height/2));

                    cv::Mat homography = cv::findHomography(modelPoints,vprojVertices_bb);

                    cv::Mat patchCurr_drift(box.size(), CV_8UC1);
                    cv::warpPerspective( *surf.mImagePtr, patchCurr_drift, homography, patchCurr_drift.size(),INTER_LINEAR, BORDER_REPLICATE);
                    
                    //to make it simple, use box previously defined,
                    //box was centered on projection of vertex in previous image
                    //now center it on previously found optimum so can use same code afterward
                    box = box + minLoc + myROI.tl() - box.tl();

                    myRoi_l = box.x-half_window_size_drift; myRoi_l = (myRoi_l<0)?0:myRoi_l;
                    myRoi_t = box.y-half_window_size_drift; myRoi_t = (myRoi_t<0)?0:myRoi_t;
                    myRoi_r = box.x+box.size().width+half_window_size_drift; myRoi_r = (myRoi_r>img.size().width)?img.size().width:myRoi_r;
                    myRoi_d = box.y+box.size().height+half_window_size_drift; myRoi_d = (myRoi_d>img.size().height)?img.size().height:myRoi_d;       

                    myROI = cv::Rect(myRoi_l,myRoi_t,myRoi_r-myRoi_l,myRoi_d-myRoi_t);//region of interest is around current position of point
                    
                    //verify that the search region is valid
                    result_cols = myROI.size().width - box.size().width + 1;
                    result_rows = myROI.size().height - box.size().height + 1;

                    if(result_cols > half_window_size_drift && result_rows > half_window_size_drift)
                    {
                        cv::Mat resultNCC_drift = cv::Mat::zeros( result_rows, result_cols, CV_32FC1 );

                        matchTemplateMI( img(myROI), patchCurr_drift, resultNCC_drift, mask);
                        
                        /// Localizing the best match with minMaxLoc searching for max NCC
                        cv::minMaxLoc( resultNCC_drift, &minVal, &maxVal, &minLoc, &maxLoc, cv::Mat() );

                        //refine maxLoc with parabolic fitting
                        cv::Point2f maxLocF;
                        parabolicRefinement(resultNCC_drift,maxLoc,maxLocF);

                        //add to list of matches
                        surfaceMatch newMatch;
                        newMatch.objectPoints = surf.center;
                        newMatch.imagePoints = maxLocF + Point2f(myROI.tl()) + vprojVertices_bbF[4];
                        newMatch.score = viewScore*maxVal;
                        mSurfaceMatches.push_back(newMatch);

                    }

                }
            }
        }
    }

    //sort out the matchesdepending on their score
    std::sort(mSurfaceMatches.begin(), mSurfaceMatches.end(), compareByScore);


    //Ransac : perform PnPwith all subsets of 4 matches, estimate pose and check corresponding score
    //keep best scoring pose
    //matches have been stored in lists: objectPoints, imagePoints and score;

    const unsigned int nbBasePnp=4;

    if(mSurfaceMatches.size() < nbBasePnp)
        return false;
    
    cv::Affine3d bestPose;
    float bestScore=0;
    int bestValidSurf=0;
    bool *inliers=new bool[mSurfaceMatches.size()];

    //do a kind of ransac: try all different subset of nbBasePnp matches to compute pose 
    //and keep the pose which returns the most inliers
    unsigned int pointers[nbBasePnp];
    for(unsigned int i=0;i<nbBasePnp;i++)pointers[i]=i;//set first pointers as first elements of list        

    //limit the number of trials:
    int cpt_trials = 0;
    int nb_trials_max = 30;
   
    while(cpt_trials<nb_trials_max)
    {
        cpt_trials ++;

        Vec3d rvec=prevPoseCam.rvec();
        Vec3d tvec=prevPoseCam.translation();
        
        //create vectors corresponding to subset
        vector<Point3f> subsetVertices;
        vector<Point2f> subsetProjections;
        for(unsigned int i=0;i<nbBasePnp;i++)
        {
            subsetVertices.push_back(mSurfaceMatches[pointers[i]].objectPoints);
            subsetProjections.push_back(mSurfaceMatches[pointers[i]].imagePoints);
        }
        
        //compute pose with subset
        cv::solvePnP(subsetVertices, subsetProjections, _mCalib.cameraMatrix, _mCalib.distCoeffs, rvec, tvec, true);

        //project all the surface centers onto image using new pose
        vector<Point3f> surfCenters;
        for(unsigned int v=0;v<mSurfaceMatches.size();v++)
            surfCenters.push_back(mSurfaceMatches[v].objectPoints);

        vector<Point2f> surfCenterProj;
        projectPoints(surfCenters, rvec, tvec, _mCalib.cameraMatrix, _mCalib.distCoeffs, surfCenterProj);

        //check how many surfaces agree with the pose and get cumulative score
        int nbValidSurf = 0;
        float scoreAccu = 0;
        bool *inliers_temp=new bool[mSurfaceMatches.size()];
        for(unsigned int v=0;v<mSurfaceMatches.size();v++)
        {
            if(cv::norm(surfCenterProj[v] - Point2f(mSurfaceMatches[v].imagePoints))<5.)
            {
                nbValidSurf++;
                scoreAccu += mSurfaceMatches[v].score;
                inliers_temp[v] = true;
            }
            else
                inliers_temp[v] = false;
        }

        //compare current pose validity with the best one so far
        //at this point we could also have used the MI and viewing angle score accumulated in scoreAccu
        if(nbValidSurf > bestValidSurf)
        {
            bestValidSurf = nbValidSurf;
            bestScore = scoreAccu;

            bestPose = cv::Affine3d(rvec,tvec);

            //save inleirs
            for(unsigned int v=0;v<mSurfaceMatches.size();v++)
                inliers[v] = inliers_temp[v];
        }
        delete[] inliers_temp;

        //if not need to go to next subset:
        bool nextSetAvailable = getNextSetPointers(&pointers[0],mSurfaceMatches.size(),nbBasePnp);

        if(!nextSetAvailable)
            break;        
    }
    
    //std::cout<<"score tracking MI= "<<bestScore<<std::endl;

    //use the MI score which has shown to be robust to occlusions
    //and illumination changes to estimate if we still track
    if(bestScore>0.4)
    {
        
        //refine pose with inliers
        Vec3d rvec=prevPoseCam.rvec();
        Vec3d tvec=prevPoseCam.translation();
        
        //create vectors corresponding to subset
        vector<Point3f> subsetVertices;
        vector<Point2f> subsetProjections;
        vector<float> scores;
        for(unsigned int i=0;i<mSurfaceMatches.size();i++)
        {
            if(inliers[i])
            {
                subsetVertices.push_back(mSurfaceMatches[i].objectPoints);
                subsetProjections.push_back(mSurfaceMatches[i].imagePoints);
                scores.push_back(mSurfaceMatches[i].score);
            }
        }
        delete[] inliers;
          
        //compute pose with all the inliers and the similarity and viewing angle score taken into account
        robustPnp(subsetVertices, subsetProjections, scores, _mCalib.cameraMatrix, _mCalib.distCoeffs, rvec, tvec);

        poseCam = cv::Affine3d(rvec,tvec);
        return true;
    }
    else
        return false;
}

}

