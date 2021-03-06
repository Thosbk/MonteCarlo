#ifndef _Point_H_
#define _Point_H_

//#include<math.h>
#include<vector>
#ifdef HAVE_MPI
#include<mpi.h>
#endif

//Class that contains every points to be computed 
class Point{
 private:
  int m_id; //id of the point
  double m_xi, m_y;
  std::string m_id_str; // id, version string
  std::string m_IdDir;
  std::string m_myDir;
  std::vector<int> m_MC; //number of simu already done (for each function)
  std::vector<int> m_MC_DoneButNotRegistered; //number of simu done but not written on disks (for each function)
  std::vector<int> m_MC_to_do; //number of simu to do (for each function)
  std::vector<int> m_NResFiles; //number of files already written (for each function)
  std::vector<double> m_average, m_stddev; //current result in memory

  static const int m_NQuantities; //Quantities to exchange with MPI

  void WriteOnFile(std::vector<std::vector<double>*> *results);

 public:
  Point(int id, double xi, double y);
  Point(Point *p);
  static void ReadAllPoints(std::vector<Point*> *PointDone);
  static void CreatePointsToDo(std::vector<Point*> *PointToDo, std::vector<Point*> *PointDone);
  // Set some values
  void SetMCDone(int ifun, int MC){m_MC[ifun] = MC;}
  void SetMC_to_do(int ifun, int MC_to_do){m_MC_to_do[ifun] = MC_to_do;}
  void SetNResFiles(int ifun, int nfiles){m_NResFiles[ifun] = nfiles;}
  void SetAverage(int ifun, int average){m_average[ifun] = average;}
  void SetStdDev(int ifun, int stddev){m_stddev[ifun] = stddev;}
  //Monte Carlo simulations
  void SetMCToDo(std::vector<int> *desired_MC); // Set the right MC_to_do, by comparing (differencing) m_MC and m_Desired_MC...
  void LaunchMC();
  void ShortCyclePlus(std::vector<double> *integrals, unsigned int *Seed);
  void PostProcessing(int ifun); // Read all the results, compute the average and standard dev
  //double uniform(){return (1+2)/(1+(double)RAND_MAX);   }
  static double uniform(unsigned int *Seed);
  static double gauss(unsigned int *Seed);
  static double f(double xi, int i);
  static double gplus(double xi, double y, int i);
  void Print();
  void Broadcast(int emitter); // receive information from emitter (mpi rank)
#if defined HAVE_MPI
  void Isend(int receiver, std::vector<MPI_Request> *request);
  void Irecv(int emitter, std::vector<MPI_Request> *request);
#endif

  //Get functions
  int GetId(){return m_id;}
  double GetXi(){return m_xi;}
  double GetY(){return m_y;}
  int GetMC(int ifun){return m_MC[ifun];}
  int GetMCToDo(int ifun){return m_MC_to_do[ifun];}
  double GetAverage(int ifun){return m_average[ifun];}
  double GetStdDev(int ifun){return m_stddev[ifun];}
  int GetNResFiles(int ifun){return m_NResFiles[ifun];}
  std::string GetIdDir(){return m_IdDir;}
  std::string GetMyDir(){return m_myDir;}
};

#endif
