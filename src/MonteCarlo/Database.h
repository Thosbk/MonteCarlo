#ifndef _Database_H_
#define _Database_H_

#include <vector>
#include <string>
#include <math.h>
#include <algorithm>
#include <MonteCarlo/Message.h>
#include <MonteCarlo/Point.h>

class Database {
 private:
  std::string m_resDir;
  std::vector <Point*> Points;
  std::vector <int>    PointsIdToDo; // Id of the points to do
  int NpointsToDo;

  //Parse the Root files: points.db, resfunXX.db, ...
  int ParseRootFiles();
  //loop on PointXX folder and parse the files and funXX folder...
  void ParsePointFiles();

  void BuildFolderPoint(int id); // Create a folder for the point with empty files
  void RebuildPointsDb(); // Rebuild points.db
  void PreparePointsToDo(); // For each points of id in "PointsIdToDo", compute the number of MC to do
  void InitResFolder(); // Create res folder and basics help files (if not exist)
  void Isend(int emitter, int receiver);
  void Irecv(int emitter, int receiver);
  void Broadcast(int rank);
 public:
  //Constructor
  Database(std::string resdir);
  ~Database();
  //Init Database: read files, build PointsDone vector, ...
  void Init();
  //Provide an array of pointers to the Points to be treated by the MC solver
  void UpdatePointsToDo(std::vector<double> *Xi, std::vector<double> *Y, std::vector<int> *MC_To_Do);
  // Launch the MC simulations for every points with id in PointsIdToDo
  void LaunchMCSimulations();
  // Post Processing
  void PostProcessing(); // Read files and write funXX.mc
  void PrintGMSHPOS(); // Write on GMSH format funXX.pos
  int FindPoint(double x, double y);
  Point* GetPointToDo(int index){return Points[PointsIdToDo[index]];}
  Point* GetPoint(int id){return Points[id];}
  //Extract and set on X,Y and res the coordinates X and Y of the points and the result (average ?)
  void ExtractXYRes(std::vector<double> *X,std::vector<double> *Y,std::vector<std::vector<double> > *res);
  //Print information on every points
  void PrintPoints();

};

#endif
