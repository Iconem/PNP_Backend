#include <unistd.h>

#include <opencv2/calib3d.hpp>

#include <iostream>

#include <math.h>

#define PI 3.14159265

using namespace std;

/*
* Convertit la matrice de rotation dans les coordonnées blender
*/
cv::Mat toBlenderCoord(const cv::Mat& R)
{
	cv::Mat P;
	if(R.depth() == CV_32F)
		P = (cv::Mat_<float>(3, 3) << 1, 0, 0, 0, -1, 0, 0, 0, -1);
	else if(R.depth() == CV_64F)
		P = (cv::Mat_<double>(3, 3) << 1, 0, 0, 0, -1, 0, 0, 0, -1);
	else
		cout << "Matrix should be CV_32F or CV_64F" << endl;
	return R*P;
}


/*
* Angles d'Euler
*/
cv::Vec3f euler(const cv::Mat& R)
{
        cv::Vec3f euler;
	if(R.depth() == CV_32F)
	{
		euler[0] = atan2(R.at<float>(2,1), R.at<float>(2,2));
		euler[1] = atan2(-R.at<float>(2,0), sqrt(R.at<float>(2,1)*R.at<float>(2,1) + R.at<float>(2,2)*R.at<float>(2,2)));
		euler[2] = atan2(R.at<float>(1,0), R.at<float>(0,0));
	}
	else if(R.depth() == CV_64F)
	{
		euler[0] = atan2(R.at<double>(2,1), R.at<double>(2,2));
		euler[1] = atan2(-R.at<double>(2,0), sqrt(R.at<double>(2,1)*R.at<double>(2,1) + R.at<double>(2,2)*R.at<double>(2,2)));
		euler[2] = atan2(R.at<double>(1,0), R.at<double>(0,0));
	}
	else
		cerr << "Matrix should be CV_32F or CV_64F" << endl;
        return euler;
}

/*
* Changement de base : position de la camera dans le systeme de coordonnées global 
*/
cv::Vec3f changeCoord(const cv::Mat& camRot, const cv::Mat& tvec, cv::Vec3f pos)
{
	cv::Mat rotation;
	if(camRot.cols == 1)
		cv::Rodrigues(camRot, rotation);
	else
		rotation = camRot;

	cv::Mat mPos = cv::Mat(pos);
	mPos.convertTo(mPos, tvec.depth());
	mPos = mPos - tvec;
	mPos.convertTo(mPos, rotation.depth());
	mPos = rotation.t() * mPos;
	cv::Vec3f camPos(mPos);
	return camPos;
}


/*
* Position d'une camera ortho a partir des correspondances 2D-3D
*
* 	u   r1|t1   X
* Z0 *	v = r2|t2 * Y
*	1   0 |1    Z
* 		    1
*
* Résoud
* A*X = Y, avec 
* Y = (u1,...un, v1,..., vn)
* A = (x1, y1, z1, 1, 0, 0, 0, 0 
*      ...
*      0, 0, 0, 0, xn, yn, zn, 1)
* donc X = 1/Z0 * (r1, t1, r2, t2)
*  
*/
double solveOrtho (
	const vector<cv::Vec3f>& xyz, 
	const vector<cv::Vec2f>& uv, 
	cv::Mat& rvec, 
	cv::Mat& tvec)
{
	int n = xyz.size();
	//*Calcul de A (Mat 2nx8)
	cv::Mat A = cv::Mat::zeros(2 * n, 8, CV_32F);
	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			A.at<float>(i, j) = xyz[i][j];
			A.at<float>(n + i, 4 + j) = xyz[i][j];
		}
		A.at<float>(i, 3) = 1;
		A.at<float>(n + i, 7) = 1;
	}
	//*Calcul de Y
	cv::Mat Y = cv::Mat::zeros(2 * n, 1, CV_32F);
	for (int i = 0; i < n; ++i)
	{
		Y.at<float>(i, 0) = uv[i][0];
		Y.at<float>(n + i, 0) = uv[i][1];
	}
	//*Resolution de AX = Y 
	//(DECOMP_SVD permet d'avoir un systeme surdéfini : http://docs.opencv.org/3.1.0/d2/de8/group__core__array.html)
	cv::Mat X;
	cv::solve(A, Y, X, cv::DECOMP_SVD);
	//*Rotation : R_brut = (r1;r2;0) = USVt, R = UVt
	cv::Mat R_brut = cv::Mat::zeros(3, 3, CV_32F);
	for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 3; ++j)
		R_brut.at<float>(i, j) = X.at<float>(4*i+j, 0);
	cv::Mat S, U, Vt;
        cv::SVD::compute(R_brut, S, U, Vt);
	cv::Mat R = U*Vt;
	if(cv::determinant(R) < 0){
                cv::Mat Z1 = (cv::Mat_<float>(3, 3) << 1, 0, 0, 0, 1, 0, 0, 0, -1);
                R = U * Z1 * Vt;
        } 
	//*Translation
	cv::Mat t = cv::Mat::zeros(3, 1, CV_32F);
	t.at<float>(0, 0) = X.at<float>(3, 0) / S.at<float>(0,0);
	t.at<float>(1, 0) = X.at<float>(7, 0) / S.at<float>(0,1);
	rvec = R;
	tvec = t;
	//calcul de l'erreur
	cv::Mat X_reproj = cv::Mat::zeros(8,1, CV_32F);
	for (int i = 0; i < 3; ++i){
		X_reproj.at<float>(i,0) = S.at<float>(0,0) * R.at<float>(0,i);
		X_reproj.at<float>(4+i,0) = S.at<float>(0,1) * R.at<float>(1,i);
	}
	X_reproj.at<float>(3,0) = X.at<float>(3,0);
	X_reproj.at<float>(7,0) = X.at<float>(7,0);
	cv::Mat Y_err = Y-A*X;
	return sqrt(Y_err.dot(Y_err));
}

/*
* Calcule la position d'une camera à partir de correspondances 2D-3D
* 
* usage : camCalib [-o] n pts 
* -o : option camera orthographique
* n : nombre de paires de points
* pts : coordonées 3d puis 2d des paires de points
* Les coordonnées 2D doivent être normalisées (x,y dans [-0.5,0.5], origine au centre, x vers la droite, y vers le haut)
*/
int main (int argc, char* argv[])
{
	//lecture des arguments
	int opt;
	bool ortho = false;
	while ((opt = getopt(argc, argv, "o-")) != -1)
	{
		switch (opt)
		{
			case 'o':
				ortho = true;
				break;
			case '-':
				break;
			default: 
				cout << "camCalibNode v2. Usage: " << argv[0] << " n pts [-o]" << endl;
				return 0;
		}
	}
	if (argc - optind < 1)
	{
		cout << "camCalibNode v2. Usage: " << argv[0] << " n pts [-o]" << endl;
		return 0;
	}	

	//lecture des correspondances 2d-3d
	vector<cv::Vec3f> xyz;
	vector<cv::Vec2f> uv;
	int n = atoi(argv[optind]);
	for (int i = 0; i < n; ++i)
	{
		cv::Vec3f obj (atof(argv[optind+5*i+1]), atof(argv[optind+5*i+2]), atof(argv[optind+5*i+3]));
		cv::Vec2f pt (atof(argv[optind+5*i+4]), atof(argv[optind+5*i+5]));
      		xyz.push_back(obj);
      		uv.push_back(pt);
	}
	
	//moyenne des points 3D (calculs plus précis)
	cv::Vec3f moyenne(0,0,0);
	for (int i = 0; i < xyz.size(); ++i)
	{
		moyenne += xyz[i];
	}
	moyenne = 1./xyz.size() * moyenne;
		
	for (int i = 0; i < xyz.size(); ++i)
	{
	 	xyz[i] -= moyenne;
	}
	
	cv::Mat rvec;
	cv::Mat tvec;

	double err = 0;
	
	vector<cv::Mat> rvecs, tvecs;
	vector<vector<cv::Vec3f> > xyzs;
	vector<vector<cv::Vec2f> > uvs;
	xyzs.push_back(xyz);
	uvs.push_back(uv);
	
	// matrice intrinseque initiale de la camera : 
	// * I3 si perspective
	// * [I2|0;0] si ortho
	// pas à l'echelle de l'image, avec le centre optique au mileu 
	cv::Mat camMatrix = cv::Mat::eye(3, 3, CV_32F);
	if (ortho)
		camMatrix.at<float>(2,2) = 0.;
	//distortion
	cv::Mat distCoeffs = cv::Mat::zeros(8, 1, CV_64F);
	
	cv::Mat camRot; //matrice de rotation
	cv::Vec3f camPos; //position
	cv::Vec3f e; //angles d'euler

	//resolution de l'equation	
	if(ortho)
	{
		err = solveOrtho(xyz, uv, rvec, tvec);
		
		camRot = rvec;
	}
	else
	{
		//caliration de la camera, avec calcul de la distortion
		err = cv::calibrateCamera(xyzs, uvs, cv::Size(1,1), camMatrix, distCoeffs, rvecs, tvecs, CV_CALIB_USE_INTRINSIC_GUESS
/*|CV_CALIB_FIX_PRINCIPAL_POINT*/
|CV_CALIB_ZERO_TANGENT_DIST
|CV_CALIB_FIX_K1
|CV_CALIB_FIX_K2
|CV_CALIB_FIX_K3);
		//Recalcul, avec matrice intrinseque fixée (ne devrait pas changer le résultat)
		cv::solvePnP(xyz, uv, camMatrix, distCoeffs, rvec, tvec);
		
		cv::Rodrigues(rvec, camRot);
	}
	//translation : 
	//pos_camera = R.t * (0-t_camera)
	camPos = changeCoord(camRot, tvec, cv::Vec3f(0, 0, 0));

	//rotation :
	//-rotation autour de l'axe X car les axes Y et Z de Blender sont opposés à ceux d'OpenCV
	//-camera.rotation = R^-1
	if(ortho)
		camRot = camRot.t();	
	else	
		camRot = toBlenderCoord(camRot.t());
	
	e = euler(camRot); 


	if(ortho)
	{
		//calcul de scale = ||K * R * (XYZ-camPos)||/||UV-0||
		cv::Mat dstModel = camMatrix * camRot * (cv::Mat) (xyz[0] - camPos);
		cout 	<< (camPos + moyenne)[0] << " " << (camPos + moyenne)[1] << " " << (camPos + moyenne)[2] << " " //position
			<< e[0] << " " << e[1] << " " << e[2] << " " //rotation
			<< sqrt(dstModel.dot(dstModel)) << " " //zoom
			<< err << endl; //err
	}
	else
	{
		// //calcul de la distance focale et du décalage du centre optique : v0 (retourne fx*35 et fy*35 ?)
		// cout 	<< (camPos + moyenne)[0] << " " << (camPos + moyenne)[1] << " " << (camPos + moyenne)[2] << " " //position
		// 	<< e[0] << " " << e[1] << " " << e[2] << " " //rotation
		// 	<< 35 * camMatrix.at<double>(0,0) << " " //focal if w > h (sensor assumed at 35)
		// 	<< 35 * camMatrix.at<double>(1,1) << " " //focal if h > w (sensor assumed at 35)
		// 	<< -camMatrix.at<double>(0,2) << " " //cx
		// 	<< camMatrix.at<double>(1,2) << " " //cy
		// 	<< err << endl; //err

		//calcul de la distance focale et du décalage du centre optique : v1 
		// (retourne fx et fy normalisés => fx_px = fx * imageWidth_px)
		cout 	<< (camPos + moyenne)[0] << " " << (camPos + moyenne)[1] << " " << (camPos + moyenne)[2] << " " //position
			<< e[0] << " " << e[1] << " " << e[2] << " " //rotation
			<< camMatrix.at<double>(0,0) << " " // (fx normalisé => fx_px = fx * imageWidth_px)
			<< camMatrix.at<double>(1,1) << " " // (fy normalisé => fy_px = fy * imageHeight_px)
			<< -camMatrix.at<double>(0,2) << " " //cx
			<< camMatrix.at<double>(1,2) << " " //cy
			<< 2 * atan( 1 / (2 * camMatrix.at<double>(1,1))) * 180 / PI // vfov
			<< err << endl; //err
	}	

	return 0;
  
}
