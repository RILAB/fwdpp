/*
  \include bneck_selection.cc
  
  Bottleneck + exponential recovery.  With selection.  Individual-based
 */

#include <fwdpp/diploid.hh>
#include <Sequence/SimData.hpp>
#include <numeric>
#include <cmath>
#include <functional>
#include <cassert>
#include <iomanip>

//the type of mutation
typedef KTfwd::mutation mtype;
#include <common_ind.hpp>

using namespace KTfwd;

//The mutation model returns an object of type KTfwd::mutation
KTfwd::mutation neutral_mutations_selection(gsl_rng * r,mlist * mutations,
					    const double & mu_neutral, const double & mu_selected,
					    const double & s, const double & h,
					    lookup_table_type * lookup)
{
  /*
    Choose a mutation position [0,1) that does not currently exist in the population.
  */
  double pos = gsl_rng_uniform(r);
  while( lookup->find(pos) != lookup->end() )
    {
      pos = gsl_rng_uniform(r);
    }
  lookup->insert(pos);
  assert(std::find_if(mutations->begin(),mutations->end(),std::bind(KTfwd::mutation_at_pos(),std::placeholders::_1,pos)) == mutations->end());
  //Choose mutation class (neutral or selected) proportional to underlying mutation rates
  bool neutral_mut = ( gsl_rng_uniform(r) <= mu_neutral/(mu_neutral+mu_selected) ) ? true : false;
  //Return a mutation of the correct type.  Neutral means s = h = 0, selected means s=s and h=h.
  return (neutral_mut == true) ? KTfwd::mutation(pos,0.,1,0.) : KTfwd::mutation(pos,s,1,h);
}

int main(int argc, char ** argv)
{
  if( argc != 14 )
    {
      std::cerr << "Error, too few arguments.\n"
		<< "Usage: " << argv[0] << ' '
		<< "N theta_neutral theta_deleterious 4Nr s h ngens N2 N3 ngens2 n nreps seed\n";
      exit(10);
    }
  int argument=1;
  const unsigned N = atoi(argv[argument++]);
  const double theta_neutral = atof(argv[argument++]);
  const double theta_del = atof(argv[argument++]);
  const double rho = atof(argv[argument++]);
  const double s = atof(argv[argument++]);
  const double h = atof(argv[argument++]);
  const unsigned ngens = atoi(argv[argument++]);
  const unsigned N2 = atoi(argv[argument++]);  //change N to N2 after ngens of evolution
  const unsigned N3 = atoi(argv[argument++]);  //N2 will change to N2 during ngens2 of exp. growth
  const unsigned ngens2 = atoi(argv[argument++]);
  const unsigned samplesize1 = atoi(argv[argument++]);
  int nreps = atoi(argv[argument++]);
  const unsigned seed = atoi(argv[argument++]);

  const double mu_neutral = theta_neutral/double(4*N);
  const double mu_del = theta_del/double(4*N);
  const double littler = rho/double(4*N);

  //Do some basic argument checking
  if(N2 > N)
    {
      std::cerr << "Error, N2 > N (" << N2 << " > " << N << "), but it should be N2 <= N\n";
    }
  if(N2 > N3)
    {
      std::cerr << "Error, N3 > N2 (" << N3 << " > " << N2<< "), but it should be N3 > N2\n";
    }
  if(ngens2 == 0)
    {
      std::cerr << "Error, ngens2 equals zero.  Must be > 0\n";
    }
  std::copy(argv,argv+argc,std::ostream_iterator<char*>(std::cerr," "));
  std::cerr << '\n';

  gsl_rng * r =  gsl_rng_alloc(gsl_rng_taus2);
  gsl_rng_set(r,seed);

  //recombination map is uniform[0,1)
  std::function<double(void)> recmap = std::bind(gsl_rng_uniform,r);

  unsigned twoN = 2*N;
  while(nreps--)
    {
      //population begins monomorphic w/1 gamete and no mutations
      glist gametes(1,gtype(twoN));
      std::vector< std::pair< glist::iterator, glist::iterator > > diploids(N,
									    std::make_pair(gametes.begin(),
											   gametes.begin()));
      mlist mutations;
      std::vector<mtype> fixations;
      std::vector<unsigned> fixation_times;
      unsigned generation;

      lookup_table_type lookup;
      double wbar=1;
      for( generation = 0; generation < ngens; ++generation )
	{
#ifndef NDEBUG
	  for( glist::iterator itr = gametes.begin(); 
	       itr != gametes.end() ; ++itr )
	    {
	      assert( itr->n > 0 );
	    }
#endif
	  assert(KTfwd::check_sum(gametes,2*N));
	  wbar = KTfwd::sample_diploid(r,
				       &gametes,
				       &diploids,
				       &mutations,
				       N,
				       mu_neutral+mu_del,
				       std::bind(neutral_mutations_selection,r,std::placeholders::_1,mu_neutral,mu_del,s,h,&lookup),
     				       std::bind(KTfwd::genetics101(),std::placeholders::_1,std::placeholders::_2,
						   &gametes,
      						   littler,
      						   r,
      						   recmap),
				       std::bind(KTfwd::insert_at_end<mtype,mlist>,std::placeholders::_1,std::placeholders::_2),
				       std::bind(KTfwd::insert_at_end<gtype,glist>,std::placeholders::_1,std::placeholders::_2),
				       std::bind(KTfwd::multiplicative_diploid(),std::placeholders::_1,std::placeholders::_2,2.),
				       std::bind(KTfwd::mutation_remover(),std::placeholders::_1,0,2*N));
	  KTfwd::remove_fixed_lost(&mutations,&fixations,&fixation_times,&lookup,generation,2*N);
	  assert(KTfwd::check_sum(gametes,2*N));
	}

      //The next generation is the bottleneck
      wbar = KTfwd::sample_diploid(r,
				   &gametes,
				   &diploids,
				   &mutations,
				   N,
				   N2,
				   mu_neutral+mu_del,
				   std::bind(neutral_mutations_selection,r,std::placeholders::_1,mu_neutral,mu_del,s,h,&lookup),
				   std::bind(KTfwd::genetics101(),std::placeholders::_1,std::placeholders::_2,
					       &gametes,
					       littler,
					       r,
					       recmap),
				   std::bind(KTfwd::insert_at_end<mtype,mlist>,std::placeholders::_1,std::placeholders::_2),
				   std::bind(KTfwd::insert_at_end<gtype,glist>,std::placeholders::_1,std::placeholders::_2),
				   std::bind(KTfwd::multiplicative_diploid(),std::placeholders::_1,std::placeholders::_2,2.),
				   std::bind(KTfwd::mutation_remover(),std::placeholders::_1,0,2*N2));
      KTfwd::remove_fixed_lost(&mutations,&fixations,&fixation_times,&lookup,generation,2*N2);
      generation++;
      
      //Figure out the growth rate, etc.
      double G = std::exp( (std::log(double(N3)) - std::log(double(N2)))/double(ngens2)); 

      //Now, grow the population to size N3;
      unsigned currentN = N2,nextN;
      unsigned gens_since_bneck = 0;
      for( ; generation < ngens + ngens2 ; ++generation,++gens_since_bneck )
	{
	  nextN = round( N2*std::pow(G,gens_since_bneck+1) );
	  assert(nextN > 0);
	  wbar = KTfwd::sample_diploid(r,
				       &gametes,
				       &diploids,
				       &mutations,
				       currentN,
				       nextN,
				       mu_neutral+mu_del,
				       std::bind(neutral_mutations_selection,r,std::placeholders::_1,mu_neutral,mu_del,s,h,&lookup),
     				       std::bind(KTfwd::genetics101(),std::placeholders::_1,std::placeholders::_2,
						   &gametes,
      						   littler,
      						   r,
      						   recmap),
				       std::bind(KTfwd::insert_at_end<mtype,mlist>,std::placeholders::_1,std::placeholders::_2),
				       std::bind(KTfwd::insert_at_end<gtype,glist>,std::placeholders::_1,std::placeholders::_2),
				       std::bind(KTfwd::multiplicative_diploid(),std::placeholders::_1,std::placeholders::_2,2.),
				       std::bind(KTfwd::mutation_remover(),std::placeholders::_1,0,2*nextN));
	  KTfwd::remove_fixed_lost(&mutations,&fixations,&fixation_times,&lookup,generation,2*nextN);
	  currentN=nextN;
	}
      Sequence::SimData neutral_muts,selected_muts;
      
      //Take a sample of size samplesize1.  Two data blocks are returned, one for neutral mutations, and one for selected
      std::pair< std::vector< std::pair<double,std::string> >,
		 std::vector< std::pair<double,std::string> > > sample = ms_sample_separate(r,&diploids,samplesize1);
      
      neutral_muts.assign( sample.first.begin(), sample.first.end() );
      selected_muts.assign( sample.second.begin(), sample.second.end() );
      
      std::cout << neutral_muts << '\n' << selected_muts << '\n';
    }
}
