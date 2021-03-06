# Thymio Tracker

Thymio tracker is a C++ visual tracker for AR Thymio.
The only dependency is OpenCV. The code has been tested using OpenCV-3.1.0.

## Default Tracker Functionalities

Thymio tracker can be divided into two main tracking applications:
 - robot tracking, 
 - landmarks tracking.

As the main objective is to do AR, the tracker has to deliver the 3D pose of 
the camera with respect to the robot and landmarks. To do so the camera has to
be calibrated.

### Camera Calibration

ThymioTracker uses the standard openCV xml calibration files. 

- On a desktop: to generate such a calibration, use either openCV's example codes, 
or use the ./calibrate binary. The ./calibrate binary uses the online capture, it detects 
the assymetric grid pattern pattern-a4.pdf which can be found in the data folder, if a few 
detection are succefull in a row, then the image is stored to compute the calibration later.
This is repeated until enough calibration images are acquired.

Once enough images are acquired the calibration is computed and stored in 
the xml output file which path has to be provided as only parameter to the program.
The path to the external calibration can then be defined in the Config.xml file
which is loaded by the ThymioTracker object (See the Config.xml example file).

- On a mobile device: to estimate and generate the calibration parameters and file, the tracker can be
used as the landmarks can be used as calibration paterns. To do so, at the creation of the ThymioTracker 
object, a CalibrationInfo object is also created which can be used to estimate the calibration online. 
For this run the tracker as described in the following sections, once you want to integrate a new image
and the information from the tracker corresponding to it to the calibration constraints (sets of 3D to 2D 
matches) use the function upadteCalibration() from the tracker. Once the number of frames asked before
calibration (nbFramesForCalibration) is reached  (getNbFramesToCalibration() == 0) then the calibration 
of the tracker is updated and the calibration file can be retrieved using the writeCalibration function.


### Robot Detection and tracking

The robot detection approach is based on the blob structure of the top surface of the robot.
If the pose of the robot at the previous frame is not known, then the blobs of the current 
image are extracted. 

To estimate if the robot blobs from the model correspond to the detected blobs there 
need to be some matching. As no texture information is available from the patch surrounding 
the blobs, the standard feature descriptors are useless. To create a feature descriptor 
suitable in this case, we use geometric hashing: the descriptor of each feature is defined 
using the geometric organization of the neigboring features [HJ Wolfson, Geometric Hashing:
An Overview]. To make the matching process faster, not all the blobs detected in an image 
are used in the GH method: as the model follows a very specific pattern (they are grouped in 
isosceles right triangle or squares), a method has been designed to use this pattern and select 
only the blobs from the current image which suit this pattern.
As the blob detectors gives us an estimation of the position of the blob in the image as well as
its scale, not only the position has been used in GH (like in most GH approaches), but the scale 
is also taken into account resulting in a 3D LUT in the GH method.

The descriptors of each blob of the model has been precomputed using the trainGH program,
see the next section to change the blob configuration (the model) and therefore update the descriptors.
As the calibration, the geometric hashing based descriptors are stored in an xml file 
accessed throught config.xml or given directly to ThymioTracker as Filestorage parameter.

Once the blobs are matched, a RANSAC-like approach is used to find the corresponding 
robot's pose and the robot tracking can start.

As the geometric hashing matching is not robust to large perspective deformation and
as the blobs extraction is also sensitive to the fronto-parallelity of the top surface,
the detection approach would not be suitable to estimate a consistent robot pose throughout
a sequence. For this another approach, which takes advantage of the temporal consistency,
is used for tracking.

To track the robot, a set of planar surfaces having some texture information have been 
defined and learned to model the robot's appearance. As in the tracking context the pose 
at the previous frame is known, we can use this pose to approximate the current robot
location. This pose is used to project each model surface onto its approximated location.
Those surfaces are then used in a tracking approach which can be divided in two steps:
- frame to frame tracking of the surface. We use the texture of the surface as it 
appeared in the previous frame and search it in the current frame using the NCC similarity
measure. This has the advantage of being robust to illumination variations but has the 
disadvantage of drifting,
- model based location refinement. To correct the drift problem of the frame to frame tracking 
approach, the model texture is reprojected onto the current estimated location in the current
image, and we search for the location which best matches the model and current image in terms
of mutual information (MI). MI is used here as it has been shown to be robust to illumination
variations, occlusions and give a nice picky optimum. 

Once we estimated the center location of each model surface in the current image, those are 
used to do a RANSAC-like pose estimation. One specificity here is that each surface match
is weighted in the PnP optimisation depending on how much information it provides and how
much fronto-parallel it is.

To estimate if the tracker diverged, a score based on the sum of the pose computation inliers
mutual information is used and thresholded. If the sum of the information provided by all our
accepted matches is below a threshold, then we consider that the tracker lost.

### Landmarks detection and tracking

To detect the landmarks, we use the BRISK feature point extraction and matching method which 
is fast and robust in the case of textured landmarks as the ones we use. As the detection is
still quite demanding in terms of computational load, the detection task is performed once every 
20 frames if the landmark is not currently tracked.

While doing a reasonable job at detecting, the BRISK approach does not use the temporal consistancy
available in our image sequences and therefore perform too pourly in terms of tracking for AR.
To alleviate the problem, a separate tracking approach has been designed: if a landmark was found 
through detection or tracking at the previous frame, the previous pose of the landmark is used to
do frame to frame tracking, doing the hypothesis that the movement in the image should not be too large.

For robustness and speed reasons, first the KLT approach is used for all the detected keypoints. This 
means that each keypoint is searched in the current image using its appearance in the previous frame
(and not the landmark reference appearance). This approach has the advantage to be fast to compute and
robust to  progressive illumination changes. However this approach has the disadvantage to be sensitive 
to drift. To correct the drift problem a subsample of the tracked features is picked randomly, and, for 
each of those features points, the estimation of its current location is refined using the model appearance
warped and "mipmapped " to the current landmark pose and an NCC template matching approach. As for every 
frame a new subset is picked, after a few tracked frames, all the landmark features will have been visited and 
their drift corrected. At every frame, the tracker also searches for new features from the model to track and 
for non-valid tracked features: a subsample of the non-detected feature points are searched in the current image 
using NCC similarity measure and a subsample of the tracked ones are checked to verify if their NCC score is high 
enough to continue the tracking.

At this stage we end up with a set of tracked reference keypoints and their estimated location in the 
current image. A RANSAC and PnP method is used then estimate the 3D pose of each landmark with respect 
to the camera.


## Definition of new tracking models

This section explains how to use the tracker for new landmarks or for a different
robot model.

### New robot model

#### New blob configuration

As the geometric hashing method used for the robot detection uses the blob configuration,
the hash table of the geometric hashing has to be updated. To do so, when the blob configuration
has been modified in Mdels.cpp (function ThymioBlobModel::setBlobModel()), use the ./trainGH program.

#### New appearance model

As the tracking works using a set of planar surfaces which texture is learned, if the appearance
of the robot changes or if one wants to use more planar surfaces, then the texture of the planar
surfaces will have to be updated.

To update the surfaces texture model, use the ./onBoard program. This program will need as input 
an image sequence and the corresponding poses of the robot (the robot to camera homogeneous 
transformation matrix). The model which is currently defined in the modelSurfaces.xml.gz file
has been acquired using aruco library and an AR board (aruco has also as only dependency OpenCV).

### New landmarks

To be able to track new landmark, use the ./landmark program from the tools folder.
The first parameter is the input image of the new landmark, the second parameter
is the the xml or xml.gz output file, the third parameter is the scale with which the image is 
going to be rescaled with before extracting the BRISK keypoint which will be used for the detection
(such rescaling is well suited in our problem as we often have as a template an image with many details
which most of the time won't be seen in our acquired images. Prefer to use this scale factor instead of
rescaling by hand as this scaling method uses gaussian pyramids and is not a simple nearest neigbor 
subsampling. One setting which worked well in terms of robustness and time consuption was to use a scaling 
factor of 0.5 with an input image of size 300x300 resulting in a rescaled image of about 150x150 pixels 
used by BRISK). The 4th and 5th parameters are the width and height of the template in meters in the real 
world. 
Once the xml or xml.gz file is created simply add it to the Config.xml file loaded by the ThymioTracker object.
