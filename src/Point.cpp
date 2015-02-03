#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <time.h>
#include <sstream> //for osstream
#include <algorithm>

#include "Message.h"
#include "Point.h"

using namespace std;

//constructor
Point::Point(int id, double xi, double y){
  m_id = id;
  m_xi = xi;
  m_y = y;
  m_MC.resize(Message::GetNFUN());
  m_MC_to_do.resize(Message::GetNFUN());
  m_NResFiles.resize(Message::GetNFUN());
  m_average.resize(Message::GetNFUN());
  m_stddev.resize(Message::GetNFUN());
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
      Message::Debug("SetMCToDo, Point Id %d, ifun = %d, MC done= %d, MC desired = %d, MC to do = %d", m_id, ifun, m_MC[ifun], (*desired_MC)[ifun], m_MC_to_do[ifun]);
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
  for (int i = 0 ; i < NFUN ; i ++)
    MC_MAX = max(MC_MAX, m_MC_to_do[i]);
  Message::Info("[Proc %d] I will do %d MC tests on point %g %g", Message::GetRank(), MC_MAX, m_xi, m_y);
  //Prepare Aux file
  std::vector<std::string> auxFile(NFUN);
  std::vector<std::ofstream *> fRes(NFUN);
  for (int i = 0; i < NFUN; i++)
    {
	  fRes[i] = new ofstream;
      std::ostringstream oss;
      oss << i;
      auxFile[i] = Message::GetResDir() + m_IdDir + "res_aux" + oss.str();
//	  std::ofstream (*(fRes[i]))(auxFile[i].c_str());
      fRes[i]->open(auxFile[i].c_str()); // TO CHANGE!!!!!!!!!
      if(!fRes[i]->is_open()) Message::Warning("Problem opening file \"%s\"", auxFile[i].c_str()); // TO CHECK
    }
  //#pragma omp parallel for private(imc)
  for (int imc = 0 ; imc < MC_MAX ; imc++)
    {
      std::vector<double> res_int;
      ShortCyclePlus(&res_int);
      //print on aux_files
      for (int i = 0; i < NFUN; i++)
		*(fRes[i]) <<  res_int[i] << "\n";
    }
  for (int i = 0; i < NFUN; i++)
    fRes[i]->close();
  //Concatenate auxilaries file
  for (int i = 0 ; i < NFUN ; i++)
    {
      std::ostringstream oss;
      oss << i;
      std::string command, resFile;
      resFile = Message::GetResDir() + m_IdDir + "res" + oss.str();
      command = "cat " + resFile + " "+ auxFile[i]; // TO CHECK
      system(command.c_str());
    }
  Message::Info("[Proc %d] Finnished %d MC tests on point %g %g", Message::GetRank(), MC_MAX, m_xi, m_y);

  //Cleaning
  for (int i = 0; i < NFUN; i++)
	  delete fRes[i];
}

void Point::ShortCyclePlus(std::vector<double> *integrals)
{
  int NFUN = Message::GetNFUN();
  integrals->resize(NFUN);
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
      double g1 = gauss();
      double g2 = gauss();
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

double Point::gplus(double xi, double y, int i)
{
  if( i == 0) return f(xi, i)*f(y, i);
  else return -1.;
}

double Point::f(double xi, int i){ 
  if(i == 0) return exp(-xi*xi);
  else return -1.;
}


/*


//Check if this point has already been done or not
void Point::Check()
{
  int nres = MyResults::GetNRES();
  for (int i = 0; i < nres; i++)
    {
      double x=MyResults::GetX(i), y = MyResults::GetY(i);
      if(_xi == x && m_y == y)
      {
	int id = MyResults::GetId(i), MC = MyResults::GetMC(i);
	int MC_to_do = m_MC - MC;
	this->SetId(id);
	if(MC_to_do <= 0)
	  Message::Warning("[Id %d] Already exist with desired number of simulations", id);
	else
	  Message::Warning("[Id %d] Already exist, only %d simu to reach %d", id, MC_to_do, m_MC);
	this->Set(_xi, m_y, MC_to_do);
      }
    }  
  if(_id == -1) //Set new id
    this->SetNewId();
  return;
}

//Ask MyResults for a new id and set it to the current point
void Point::SetNewId()
{
  int id = MyResults::GetNewId();
  m_id = id;
  Message::Info(2,"New ID provided");
  return;
}


void Point::PrepareMyFile(char *res_dir, int *file_id)
{
  std::cout.precision(Message::Precision());
  int id = m_id;
  double xi=_xi, y=_y;
  char command[128], res_dir_loc[128];
  sprintf(res_dir_loc, "./%s/Id%d/", res_dir, id);
  sprintf(command, "if ! test -d %s; then mkdir %s; fi", res_dir_loc, res_dir_loc);
  //  sprintf(command, "mkdir %s", res_dir_loc);
  system(command);
  //create file (search file id that is free (no erase))
  int fcpt = 0;
  char file_aux[128];
  sprintf(file_aux, "%sres%d.mc", res_dir_loc, fcpt);
  std::ifstream fNewRes_aux(file_aux);
  while(!fNewRes_aux.fail())
    {
      fNewRes_aux.close();
      fcpt++;
      sprintf(file_aux, "%sres%d.mc", res_dir_loc, fcpt);
      fNewRes_aux.open(file_aux);
    }
  *file_id = fcpt;
  
  //create/verify file containing (x,y) coordinate
  char coord_file[128];
  sprintf(coord_file, "%smyCoord.mc", res_dir_loc);
  //small checking on the coordinate of the points...
  std::ifstream fcoord_in(coord_file);
  if((fcoord_in.fail()))
    {
      std::ofstream fcoord(coord_file);
      fcoord << xi << "\n" << y;
      fcoord.close();
    }
  else{//check file
    double xi_aux, y_aux;
    fcoord_in >> xi_aux >> y_aux;
    fcoord_in.close();
    bool test_file=(xi == xi_aux && y == y_aux);
    if(!test_file)
      {//There is a (huge) problem, better keep all information in a separate file
	Message::Warning("[Id %d] Bad coordinate detected !", id);
	char coord_file_aux[128];
	sprintf(coord_file_aux, "%s_aux%d", coord_file, fcpt);
	std::ofstream fcoord(coord_file_aux);
	fcoord << xi << "\n" << y;
	fcoord.close();
      }
  }
  return;
}

void Point::LaunchMC(char *traj_dir, int seed)
{
  Message::Info("[Proc %d] I will do %d MC tests on point %g %g", Message::GetCommRank(), m_MC, m_xi, m_y);

  /*
  //change seed
  //time(NULL) - 360000*myRank
  srand(seed);
  //Create directory (if doesn't exist)
  int file_id;
  PrepareMyFile(traj_dir, &file_id);
  char traj_dir_loc[128], traj_file[128];
  sprintf(traj_dir_loc, "./%s/Id%d/", traj_dir, m_id);
  sprintf(traj_file, "%sres%d.mc", traj_dir_loc, file_id);
  std::ofstream fNewRes(traj_file);
  
  //#pragma omp parallel for private(imc)
  myStr << "$MC\n"<< m_MC << "\n";
  for (int imc = 0 ; imc < m_MC ; imc++)
    {
      Message::Info("%d",imc);
      std::vector<std::vector<double> > traj_imc;
      ShortCyclePlus(&traj_imc);
      std::stringstream myStr(ostringstream::out|ios::binary);
      myStr << traj_imc.size() << "\n";
      for (int i = 0; i < traj_imc.size(); i++)
	myStr << std::setprecision(Message::Precision()) << traj_imc[i][0] << " " << std::setprecision(Message::Precision()) << traj_imc[i][1] << "\n";      
      fNewRes << myStr.str();
    }
  fNewRes.close(); 
  */
/*}

*/

