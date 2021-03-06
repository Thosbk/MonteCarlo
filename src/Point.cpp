#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <time.h>
#include <sstream> //for osstream
#include <algorithm>
#include <math.h>

#include <MonteCarlo/Message.h>
#include <MonteCarlo/Point.h>

#ifdef HAVE_MPI
#include<mpi.h>
#endif

#ifdef HAVE_OMP
#include <omp.h>
#endif

const int Point::m_NQuantities = 5;

//constructor
Point::Point(int id, double xi, double y): m_id(id), m_xi(xi), m_y(y)
{
  m_MC.resize(Message::GetNFUN(), 0);
  m_MC_to_do.resize(Message::GetNFUN(), 0);
  m_NResFiles.resize(Message::GetNFUN(), 0);
  m_average.resize(Message::GetNFUN(), 0);
  m_stddev.resize(Message::GetNFUN(), 0);
  std::stringstream iid;
  iid << m_id;
  m_id_str = iid.str();
  m_myDir = Message::GetResDir() + Message::GetPointFolderRootName() + m_id_str + Message::GetBackSlash();
}

Point::Point(Point *p){
  m_IdDir = p->GetIdDir();
  m_id = p->GetId();
  m_xi = p->GetXi();
  m_y = p->GetY();
  m_MC.resize(Message::GetNFUN());
  m_MC_to_do.resize(Message::GetNFUN());
  m_NResFiles.resize(Message::GetNFUN());
  m_average.resize(Message::GetNFUN());
  m_stddev.resize(Message::GetNFUN());

  for(int i = 0; i < Message::GetNFUN(); i++)
    {
      m_MC[i]       = p->GetMC(i);
      m_MC_to_do[i] = p->GetMCToDo(i);
      m_NResFiles[i]= p->GetNResFiles(i);
      m_average[i]  = p->GetAverage(i);
      m_stddev[i]   = p->GetStdDev(i);
    }
}

void Point::SetMCToDo(std::vector<int> *desired_MC)
{
  for (int ifun = 0; ifun < Message::GetNFUN(); ifun ++)
    {
      if((*desired_MC)[ifun] - m_MC[ifun] > 0)
	m_MC_to_do[ifun] = (*desired_MC)[ifun] - m_MC[ifun];
      else
	m_MC_to_do[ifun] = 0;
    }  
}


void Point::Print()
{
  Message::Info("Printing point informations...");
  Message::Info("id = %d", m_id);
  Message::Info("xi = %g", m_xi);
  Message::Info("y  = %g", m_y);
}


void Point::ReadAllPoints(std::vector<Point*> *PointDone)
{
  //Read all folder and build the right Point
}

void Point::CreatePointsToDo(std::vector<Point*> *PointToDo, std::vector<Point*> *PointDone)
{
  //Use the param file to build point according to :
  // -- The given grid (param)
  // -- The already existing point (PointDone)
  //Destruction of the point must be achieved by the user !
}

void Point::LaunchMC()
{
  int MC_MAX = 0;
  const int NFUN = Message::GetNFUN();
  for (int ifun = 0 ; ifun < NFUN ; ifun ++)
    MC_MAX = std::max(MC_MAX, m_MC_to_do[ifun]);
  if(MC_MAX <= 0)
    return;
  Message::Info("Me and my %d threads will do %d MC tests on point with id %d and (xi,y) = (%g, %g)", Message::GetNumThreads(), MC_MAX, m_id, m_xi, m_y);
  //Restart is a parameter that forces the program to print on files every time 
  //a number of Restart computations have been done. 
  int Restart = Message::GetRestart();
  if(Restart <= 0)
    Restart = MC_MAX;
  int irestart_end = MC_MAX/Restart + std::min(1, MC_MAX%Restart);
  for (int irestart = 0; irestart < irestart_end; irestart ++)
    {
      int MC_start = irestart*Restart;
      int MC_end = std::min(MC_MAX, (irestart + 1)*Restart);
      int MC_currentLoop = MC_end - MC_start;
      std::vector<std::vector<double>* > resultsMC(NFUN);
      int imc;
      for (int ifun = 0; ifun < NFUN; ifun ++)
	{
	  resultsMC[ifun] = new std::vector<double>;
	  resultsMC[ifun]->reserve(MC_currentLoop); //Avoiding memory problem
	}
#pragma omp parallel private(imc)
      {
	unsigned int Seed = time(NULL) - 360000*(Message::GetRank()*Message::GetNumThreads() + Message::GetThreadNum());
	std::vector<std::vector<double>* > MyresultsMC(Message::GetNFUN());
	for (int ifun = 0; ifun < NFUN; ifun ++)
	  {
	    MyresultsMC[ifun] = new std::vector<double>;
	    MyresultsMC[ifun]->reserve(MC_currentLoop/(Message::GetNumThreads()) + Message::GetNumThreads()); //Avoiding memory problem
	  }
#pragma omp for nowait schedule(static)
	for (imc = MC_start ; imc < MC_end ; imc++)
	  {
	    std::vector<double> res_int(NFUN);
	    ShortCyclePlus(&res_int, &Seed);
	    for (int ifun = 0; ifun < NFUN; ifun ++)
	      MyresultsMC[ifun]->push_back(res_int[ifun]);
	  }
#pragma omp critical
	{
	  for(int ifun = 0; ifun < Message::GetNFUN() ; ifun++)
	    resultsMC[ifun]->insert(resultsMC[ifun]->end(), MyresultsMC[ifun]->begin(), MyresultsMC[ifun]->end());
	}
	//Cleaning
	for (int ifun = 0; ifun < NFUN; ifun ++)
	  delete MyresultsMC[ifun];

      }//end omp parallel
      //Updating files
      WriteOnFile(&resultsMC);
      //Cleaning
      for (int ifun = 0; ifun < NFUN; ifun ++)
	delete resultsMC[ifun];
    }
  Message::Info("Finished %d MC tests on point (%g,%g)", MC_MAX, m_xi, m_y);
}

void Point::ShortCyclePlus(std::vector<double> *integrals, unsigned int *Seed)
{
  int NFUN = Message::GetNFUN();
  if(integrals->size()!=NFUN)
    {
      Message::Warning("ShortCyclePlus: integrals is not of the right size! Abording...");
      Message::Finalize(EXIT_FAILURE);
    }
  for(int i = 0; i < NFUN; i++)
    (*integrals)[i] = 0.0;
  //run a trajectory starting from (xi,y) \in DELTAPLUS of the solution on the boundary.
  int cpt = 0;
  double t = 0.;
  double T = Message::GetT();
  double dt = Message::GetDt();
  double sdt = Message::GetSdt();
  double ro = Message::GetRo();
  double roc = Message::GetRoc();
  double c0 = Message::GetC();
  double k = Message::GetK();
  double Y = Message::GetY();
  double alpha = Message::GetAlpha();
  double lambda = Message::GetLambda();
  int it_max = T/dt;
  double xi = m_xi, y = m_y;
  for(int it = 0 ; it < it_max ; it++)
    {
      t += dt;
      if(t > T){ t = T;}
      //noise
      double g1 = gauss(Seed);
      double g2 = gauss(Seed);
      double xi_aux = xi, y_aux = y;
      xi += -alpha*xi_aux*dt + sdt*g1;
      y  += -(alpha*xi_aux + c0*y_aux + k*Y)*dt + sdt*(ro*g1 + roc*g2);
      //Compute integrals (Hard coded !)
      for(int i=0; i < NFUN ; i++)
	(*integrals)[i] += dt*exp(-lambda*t)*gplus(xi, y, i);
      if (y<0)
	{
	  for (int i = 0; i < NFUN ; i++)
	    (*integrals)[i] += exp(-lambda*t)*f(xi, i);
	  break;
	}
    }
  return;
}

double Point::gplus(double xi, double y, int ifun)
{
  return f(xi, ifun)*f(y, ifun);
}

double Point::f(double xi, int ifun){ 
  if(ifun == 0){
    double res = exp(-xi*xi);
    return res;
  }
  else{
    Message::Warning("This function number %d is not implemented yet, abording...", ifun);
    Message::Finalize(EXIT_FAILURE);
    return -1;
  };
}

double Point::gauss(unsigned int *Seed)
{
  double rand1 = uniform(Seed);
  double rand2 = uniform(Seed);
  double res = sqrt(-2.*log(rand1))*cos(Message::GetDeuxPi()*rand2);
  return res;
}


double Point::uniform(unsigned int *Seed)
{
  double randNum = static_cast<double>(rand_r(Seed));
  double res = (1+randNum)/(1+static_cast<double>(RAND_MAX));
  return res;
}


//Write on file and update data about the point...
void Point::WriteOnFile(std::vector<std::vector<double>*> *results)
{
  Message::Info(4, "WriteOnFile");
  int NFUN = Message::GetNFUN();
  //Index of the functions that have new results to be stored !
  std::vector<int> FunWithNewResults;
  FunWithNewResults.reserve(NFUN);
  for (int ifun = 0; ifun < NFUN; ifun ++)
    {
      if(m_MC_to_do[ifun] > 0)
	FunWithNewResults.push_back(ifun);
    }
  int NFUNWithNewRes = FunWithNewResults.size();

  //Build a new res file for this Point and every functions
  std::vector<std::ofstream *> fRes(NFUNWithNewRes);
  for (int ifunAux = 0; ifunAux < NFUNWithNewRes; ifunAux++)
    {
      int ifunId = FunWithNewResults[ifunAux];
      std::ostringstream osifun, osiNbFiles;
      osifun << ifunId;
      osiNbFiles << m_NResFiles[ifunId];
      std::string resFileName = m_myDir + Message::GetFunResFolderRootName() + osifun.str() + Message::GetBackSlash() + Message::GetPointResRootName() + osiNbFiles.str()  + Message::GetDBext();
      fRes[ifunAux] = new std::ofstream(resFileName.c_str(), std::ios_base::out); 
      if(!fRes[ifunAux]->is_open()) Message::Warning("Problem opening file \"%s\"", resFileName.c_str());
    }

  //THIS IS JUST TO DELETE SOME RESULTS TO GET THE EXACT NUMBER OF MC... YES, IT'S WEIRD !!
  std::vector<int> HowManyRes(NFUNWithNewRes);
  for (int ifunAux = 0; ifunAux < NFUNWithNewRes; ifunAux++)
    {
      int ifunId = FunWithNewResults[ifunAux];
      int sizeResults = (*results)[ifunId]->size();
      HowManyRes[ifunAux] = std::min(sizeResults, m_MC_to_do[ifunId]);
      //The first number if the number of results stored in the file
      *(fRes[ifunAux]) << HowManyRes[ifunAux] << "\n";
      m_MC[ifunId] += HowManyRes[ifunAux];
      m_MC_to_do[ifunId] -= HowManyRes[ifunAux];
    }

  //Write on files
  for (int ifunAux = 0; ifunAux < NFUNWithNewRes; ifunAux++)
    {
      int ifunId = FunWithNewResults[ifunAux];
      for (int ires =0; ires < HowManyRes[ifunAux]; ires++)
	{
	  *(fRes[ifunAux]) << (*(*results)[ifunId])[ires] << "\n";	  
	}
      fRes[ifunAux]->close();
      m_NResFiles[ifunId] ++;
    }

  //Root files
  for (int ifunAux = 0; ifunAux < NFUNWithNewRes; ifunAux++)
    {
      int ifunId = FunWithNewResults[ifunAux];
      std::ostringstream osifun;
      osifun << ifunId;
      std::string funXXName = m_myDir + Message::GetFunResRootName() +osifun.str() + Message::GetDBext();
      std::ofstream funXX(funXXName.c_str(), std::ios_base::out);
      funXX << m_NResFiles[ifunId]<< "\n";
      funXX << m_MC[ifunId]<< "\n";
      funXX.close();
    }

  //Cleaning
  for (int ifunAux = 0; ifunAux < NFUNWithNewRes; ifunAux ++)
    delete fRes[ifunAux];
}


void Point::PostProcessing(int ifun)
{
  //Folder name
  std::stringstream iifun;
  iifun << ifun;
  std::string rootFunFolder = Message::GetFunResFolderRootName() + iifun.str() + Message::GetBackSlash();
  //Prepare vector and values...
  double average =0., stddev=0.;
  std::vector<double> results;
  results.reserve(m_MC[ifun]);
  int nfiles = m_NResFiles[ifun];
  //loop on every files of funXX and this point
  //This loop should be parallelizable... (OPENMP)
  for (int ifile = 0; ifile < nfiles ; ifile ++)
    {
      std::stringstream iifile;
      iifile << ifile;
      std::string point_resXXName = m_myDir + rootFunFolder + Message::GetPointResRootName() + iifile.str() + Message::GetDBext(); 
      std::ifstream point_resXX(point_resXXName.c_str(), std::ios_base::in);
      if(!point_resXX.is_open()){
	Message::Warning("Could not open %s, abording...", point_resXXName.c_str());
	Message::Finalize(EXIT_FAILURE);
      }
      int nMC;
      point_resXX >> nMC;
      for (int iMC = 0; iMC < nMC; iMC++)
	{
	  double currentMC;
	  point_resXX >> currentMC;
	  results.push_back(currentMC);
	  average += currentMC;
	  stddev += currentMC*currentMC;
	}
      point_resXX.close();
      //Compute the average and standard deviation
    }
  average /= m_MC[ifun];
  stddev = sqrt(stddev/m_MC[ifun] - average*average);
  m_average[ifun] = average;
  m_stddev[ifun] = stddev;
}

void Point::Broadcast(int emitter)
{
#ifdef HAVE_MPI
  MPI_Bcast(&m_MC[0], m_MC.size(), MPI_INT, emitter, MPI_COMM_WORLD);
  MPI_Bcast(&m_MC_to_do[0], m_MC_to_do.size(), MPI_INT, emitter, MPI_COMM_WORLD);
  MPI_Bcast(&m_NResFiles[0], m_NResFiles.size(), MPI_INT, emitter, MPI_COMM_WORLD);
  MPI_Bcast(&m_average[0], m_average.size(), MPI_DOUBLE, emitter, MPI_COMM_WORLD);
  MPI_Bcast(&m_stddev[0], m_stddev.size(), MPI_DOUBLE, emitter, MPI_COMM_WORLD);
#endif
}

#if defined HAVE_MPI
void Point::Isend(int receiver, std::vector<MPI_Request> *request)
{
  std::vector<int> Tag(m_NQuantities);
  for (int i = 0; i < m_NQuantities; i++)
    Tag[i] = m_id*100 + i;
  int cpt = 0;
  MPI_Request m0;
  MPI_Isend(&m_MC[0], m_MC.size(), MPI_INT, receiver, Tag[cpt], MPI_COMM_WORLD, &m0); cpt ++;
  request->push_back(m0);  MPI_Request m1;
  MPI_Isend(&m_MC_to_do[0], m_MC_to_do.size(), MPI_INT, receiver, Tag[cpt], MPI_COMM_WORLD, &m1); cpt ++;
  request->push_back(m1);  MPI_Request m2;
  MPI_Isend(&m_NResFiles[0], m_NResFiles.size(), MPI_INT, receiver, Tag[cpt], MPI_COMM_WORLD, &m2); cpt ++;
  request->push_back(m2);  MPI_Request m3;
  MPI_Isend(&m_average[0], m_average.size(), MPI_DOUBLE, receiver, Tag[cpt], MPI_COMM_WORLD, &m3); cpt ++;
  request->push_back(m3);  MPI_Request m4;
  MPI_Isend(&m_stddev[0], m_stddev.size(), MPI_DOUBLE, receiver, Tag[cpt], MPI_COMM_WORLD, &m4); cpt ++;
  request->push_back(m4);
}
#endif

#if defined HAVE_MPI
void Point::Irecv(int emitter, std::vector<MPI_Request> *request)
{
  std::vector<int> Tag(m_NQuantities);
  for (int i = 0; i < m_NQuantities; i++)
    Tag[i] = m_id*100 + i;
  int cpt = 0;
  MPI_Request m0;
  MPI_Irecv(&m_MC[0], m_MC.size(), MPI_INT, emitter, Tag[cpt], MPI_COMM_WORLD, &m0); cpt ++;
  request->push_back(m0);  MPI_Request m1;
  MPI_Irecv(&m_MC_to_do[0], m_MC_to_do.size(), MPI_INT, emitter, Tag[cpt], MPI_COMM_WORLD, &m1); cpt ++;
  request->push_back(m1);  MPI_Request m2;
  MPI_Irecv(&m_NResFiles[0], m_NResFiles.size(), MPI_INT, emitter, Tag[cpt], MPI_COMM_WORLD, &m2); cpt ++;
  request->push_back(m2);  MPI_Request m3;
  MPI_Irecv(&m_average[0], m_average.size(), MPI_DOUBLE, emitter, Tag[cpt], MPI_COMM_WORLD, &m3); cpt ++;
  request->push_back(m3);  MPI_Request m4;
  MPI_Irecv(&m_stddev[0], m_stddev.size(), MPI_DOUBLE, emitter, Tag[cpt], MPI_COMM_WORLD, &m4); cpt ++;
  request->push_back(m4);
}
#endif
