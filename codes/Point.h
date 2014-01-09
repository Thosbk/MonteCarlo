#ifndef _ResToCompute_H_
#define _ResToCompute_H_

#include<math.h>


//Class that contains every points to be computed 
class Point{
 private:
  int _id; //global id, same as in Results
  double _xi, _y;
  int _MC; //number of simu to do
 public:
  static int _npoints;
  Point();
  Point(double xi, double y, int MC);
  Point(int id, double xi, double y, int MC);
  double GetX(){return _xi;};
  double GetY(){return _y;};
  int GetMC(){return _MC;};
  int GetId(){return _id;};
  void GetValues(double *xi, double *y, int *MC_loc){(*xi)=_xi; (*y)=_y; (*MC_loc)=_MC;};
  void GetValues(int *id, double *xi, double *y, int *MC_loc){(*id)=_id; GetValues(xi,y,MC_loc);};
  //set stuffs
  void SetId(int id){_id = id;};
  void Set(int id, double xi, double y, int n);
  void Set(double xi, double y, int n);
  //Check point
  void Check(void);
  //set new id
  void SetNewId(void);
  //check next file name available
  void PrepareMyFile(char *res_dir, int *file_id);
  //=========================
  //TRAJECTORIES COMPUTATION
  //=========================
  //trajectories function
  static double uniform(){return (1+(double)rand())/(1+(double)RAND_MAX);};
  //  static double uniform(){return (1+2)/(1+(double)RAND_MAX);};
  static double gauss(){return sqrt(-2.*log(uniform()))*cos(Message::GetDeuxPi()*uniform());};
  void ShortCyclePlus(std::vector<std::vector<double> > *trajectories);
  //launch the monte carlo simulations
  void LaunchMC(char *traj_dir, int seed);
  /*
  //=====
  //DATA
  //=====
  static double f(double xi){ return exp(-xi*xi);};
  static double gplus(double xi, double y){return f(xi)*f(y);};
  */
};

#endif
