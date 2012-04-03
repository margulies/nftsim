#include<cmath>
#include<string>
using std::string;
#include<iostream>
using std::endl;
#include<sstream>
using std::stringstream;

#include"solver.h"
#include"population.h"
#include"array.h"
#include"configf.h"

#include"propag.h"
#include"wave.h"
#include"harmonic.h"

#include"couple.h"
#include"cadp.h"

#include"output.h"

void Solver::CntMat::init( Configf& configf )
{
  // Read the number of populations
  configf.next("From");
  // Expect format "From: 1 2 3 4", read the 
  vector<double> pop = configf.numbers();
  npop = pop.size();

  raw.resize(npop);

  for( int i=0; i<npop; i++ ) {
    configf.next( label("To ",i+1) ); // ignore "To ?:"
    raw[i] = configf.numbers();
    if( raw[i].size() != size_t(npop) ) {
      std::cerr << "The connection matrix is not configured correctly."
        << endl;
      exit(EXIT_FAILURE);
    }
  }

  // presynaptic population index for each connection index
  for( int i=0; i<npop; i++ )
    for( int j=0; j<npop; j++ )
      if( raw[i][j] )
        pre.push_back(j);

  // postsynaptic population index for each connection index
  for( int i=0; i<npop; i++ )
    for( int j=0; j<npop; j++ )
      if( raw[i][j] )
        post.push_back(i);

  ncnt = post.size(); // == pre.size()
}

void Solver::CntMat::dump( Dumpf& dumpf ) const
{
  dumpf << "Connection matrix:" << endl
    << "From:";
  for( int i=0; i<npop; i++ )
    dumpf << " " << i;
  for( int i=0; i<npop; i++ ) {
    dumpf << endl << "To " << i+1 << ":";
    for( int j=0; j<npop; j++ )
      dumpf << " " << raw[i][j];
  }
  dumpf << endl << endl;
}

void Solver::Outputs::init( Configf& configf )
{
  // read in nodes to output
  configf.next("Node");
  if( configf.find("Node:") == "All" ) // beware of this slightly hackish line
    for( int i=0; i<nodes; i++ )
      Output::node.push_back(i);
  else {
    vector<double> temp = configf.numbers();
    for( size_t i=0; i<temp.size(); i++ )
      if( temp[i] > nodes ) {
        std::cerr<<"Trying to plot node number "<<temp[i]
            <<", which is bigger than the highest node index."<<endl;
        exit(EXIT_FAILURE);
      }
      else
        Output::node.push_back( temp[i]-1 );
  }

  // read in time to start of output
  double tempf;
  if( !configf.optional("Start",tempf) )
    start = 0;
  else {
    if( remainder(tempf,deltat) >deltat ) {
      std::cerr<<"Value of output start time not divisible by Deltat."<<endl;
      exit(EXIT_FAILURE);
    }
    else
      start = tempf/deltat;
  }
  t = 0;

  // read in output interval
  if( !configf.optional("Interval",tempf) )
    interval = 1;
  else {
    if( remainder(tempf,deltat) >deltat ) {
      std::cerr<<"Value of output interval not divisible by Deltat."<<endl;
      exit(EXIT_FAILURE);
    }
    else
      interval = tempf/deltat+.5;
  }

  // read in populations to output
  configf.next("Population");
  vector<double> temp = configf.numbers();
  for( size_t i=0; i<temp.size(); i++ ) {
    if( temp[i] > npop ) {
      std::cerr<<"Trying to print population "<<temp[i]
          <<", which is an invalid population."<<endl;
      exit(EXIT_FAILURE);
    }
    m.add( pops[temp[i]-1]->output() );
  }

  // read in propags to output
  configf.next("Propag");
  temp = configf.numbers();
  for( size_t i=0; i<temp.size(); i++ ) {
    if( temp[i] > ncnt ) {
      std::cerr<<"Trying to print propagator "<<temp[i]
          <<", which is an invalid propagator."<<endl;
      exit(EXIT_FAILURE);
    }
    m.add( propags[temp[i]-1]->output() );
  }

  // read in couples to output
  configf.next("Couple");
  temp = configf.numbers();
  for( size_t i=0; i<temp.size(); i++ ) {
    if( temp[i] > ncnt ) {
      std::cerr<<"Trying to print couple "<<temp[i]
          <<", which is an invalid couple."<<endl;
      exit(EXIT_FAILURE);
    }
    m.add( couples[temp[i]-1]->output() );
  }

  // write out first row
  Output::dumpf<<space<<"Time"<<space<<space<<septor;
  for( size_t i=0; i<m.size(); i++ ) {
    for( size_t j=0; j<Output::node.size(); j++ )
      Output::dumpf<<space<<space<<m[i]->fieldname();
    Output::dumpf<<space<<space<<septor;
  }
  // write out second row
  Output::dumpf<<endl<<space<<space<<space<<" "<<septor;
  for( size_t i=0; i<m.size(); i++ ) {
    for( size_t j=0; j<Output::node.size(); j++ )
      Output::dumpf<<space<<space<<setw<<int(j+1);
    Output::dumpf<<space<<space<<septor;
  }
  Output::dumpf<<endl;

}

void Solver::Outputs::step(void)
{
  if( start )
    start--;
  else {
    t++;
    if( t%interval==0 ) {
      Output::dumpf<<double(t)*deltat<<space<<space<<septor;
      m.step();
      Output::dumpf<<endl;
    }
  }
}

void Solver::Outputs::dump( Dumpf& dumpf ) const
{
}

Solver::Outputs::Outputs(
    Array<Population>& pops, Array<Propag>& propags, Array<Couple>& couples )
  : pops(pops), propags(propags), couples(couples)
{
}

Solver::Solver( Dumpf* dumpf )
    : NF(0,0,0), dumpf(dumpf), outputs(pops,propags,couples)
{
}

Solver::~Solver()
{
  if(dumpf) {
    *dumpf<<*this;
    delete dumpf;
  }
}

void Solver::init( Configf& configf )
{
  // Anything before the first ':' is ignored as comment

  // read in simulation time and timestep
  double tempf; configf.param("Time",tempf);
  configf.param("Deltat",deltat);
  if( remainder(tempf,deltat) >deltat ) {
    std::cerr<<"Value of total simulation time not divisible by Deltat."<<endl;
    exit(EXIT_FAILURE);
  }
  else
    steps = tempf/deltat +.5;

  // read in grid size and grid geometry
  configf.param("Nodes",nodes);
  int longside;
  if( configf.optional("Longside",longside) ) {
    if( nodes%longside != 0 ) {
      std::cerr << "To define a rectangular grid nodes: " << nodes <<endl
        << "divided by Longside: " << longside << endl
        << "must have no remainder" << endl;
      exit(EXIT_FAILURE); 
    }
  }
  else
    longside = sqrt(nodes);
  string topology("Torus");
  if( configf.optional("Topology",topology) )
    if( topology == "Nonperiodic" ) {
      double bath;
      configf.param("Bath",bath);
      stringstream ss; ss<<topology<<" "<<bath;
      topology = ss.str();
    }

  // glutamte dynamics
  glu.resize(nodes,0); dglu.resize(nodes,0);
  configf.param("Lambda",Lambda); configf.param("tGlu",tGlu);

  // read in connection matrix
  configf.next("Connection matrix"); cnt.init(configf);

  // construct populations
  for( int i=0; i<cnt.npop; i++ )
    pops.add( new Population(nodes,deltat,i) );

  for( int i=0; i<cnt.ncnt; i++ ) {
    string ptype = configf.find( label("Propag ",i+1) +":" );
    // PUT YOUR PROPAGATORS HERE
    if(ptype=="Map")
      propags.add( new
        Propag(nodes,deltat,i, *pops[cnt.pre[i]], *pops[cnt.post[i]], longside, topology));
    else if(ptype=="Wave") {
      if( nodes==1 )
      propags.add( new
        Harmonic(nodes,deltat,i, *pops[cnt.pre[i]], *pops[cnt.post[i]], longside, topology));
      else
      propags.add( new
        Wave(nodes,deltat,i, *pops[cnt.pre[i]], *pops[cnt.post[i]], longside, topology));
    }
    else if(ptype=="Harmonic")
      propags.add( new
        Harmonic(nodes,deltat,i, *pops[cnt.pre[i]], *pops[cnt.post[i]], longside, topology));
    else {
      std::cerr<<"Invalid propagator type '"<<ptype<<"'."<<endl;
      exit(EXIT_FAILURE);
    }
    // END PUT YOUR PROPAGATORS HERE

    string ctype = configf.find( label("Couple ",i+1) +":" );
    // PUT YOUR COUPLES HERE
    if(ctype=="Map")
      couples.add( new
        Couple(nodes,deltat,i, glu, *pops[cnt.pre[i]], *pops[cnt.post[i]] ) );
    else if(ctype=="CaDP")
      couples.add( new
        CaDP(nodes,deltat,i, glu, *pops[cnt.pre[i]], *pops[cnt.post[i]] ) );
    else {
      std::cerr<<"Invalid couple type '"<<ctype<<"'."<<endl;
      exit(EXIT_FAILURE);
    }
    // END PUT YOUR COUPLES HERE
  }

  // insert dendrites to each population
  for( int i=0; i<cnt.npop; i++ )
    for( int j=0; j<cnt.ncnt; j++ )
      if( cnt.post[j] == i )
        pops[i]->add2Dendrite( j, *propags[j], *couples[j] );

  // read couples parameters
  configf.go2("Couple 1");
  for( int i=0; i<cnt.ncnt; i++ )
    configf.param( label("Couple ",i+1), *couples[i] );

  // read propags parameters
  configf.go2("Propag 1");
  for( int i=0; i<cnt.ncnt; i++ )
    configf.param( label("Propag ",i+1), *propags[i] );

  // read populations parameters
  configf.go2("Population 1");
  for( int i=0; i<cnt.npop; i++ )
    configf.param( label("Population ",i+1), *pops[i] );

  // initialize outputs
  outputs.nodes = nodes; outputs.deltat = deltat;
  outputs.npop = cnt.npop; outputs.ncnt = cnt.ncnt;
  configf.go2("Output"); configf.next("Output"); outputs.init(configf);
}

void Solver::restart( Restartf& restartf )
{
}

void Solver::dump( Dumpf& dumpf ) const
{
  dumpf << pops;
  dumpf << propags;
  dumpf << couples;
  outputs.dump(dumpf);
}

void Solver::solve(void)
{
  for( int i=0; i<steps; i++ )
    step();
}

void Solver::step(void)
{
  // glutamte dynamics
  for( int j=0; j<nodes; j++ )
    dglu[j] = 0;//double ddglu = 0; double ts = 200e-3; double td = 200e-3;
  for( size_t i=0; i<couples.size(); i++ )
    if( couples[i]->excite() )
      for( int j=0; j<nodes; j++ )
        dglu[j] += Lambda*propags[i]->phi()[j]*deltat;
  for( int j=0; j<nodes; j++ ) {
    dglu[j] -= glu[j]/tGlu*deltat;
    glu[j] += dglu[j];
    if( glu[j]<0 ) glu[j] = 0;
    /*double p1 = ddglu;
      double k1 = dglu[j];
      double p2 = p1 -p1/2/ts -k1/2/td;
      double k2 = dglu[j] +1/2*(dglu[j]+p1);
      double p3 = p1 -p2/2/ts -k2/2/td;
      double k3 = dglu[j] +1/2*(dglu[j]+p2);
      double p4 = p1 -p3/ts -k3/td;
      double k4 = dglu[j] +dglu[j]+p3;
      dglu[j] += 1/6*(p1+2*p2+2*p3+p4);
      glu[j] += 1/6*(k1+2*k2+2*k3+k4);
    */
  }

  couples.step();
  pops.step();
  propags.step();
  outputs.step();
}
