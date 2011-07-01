/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file   GaussianBayesNet.cpp
 * @brief  Chordal Bayes Net, the result of eliminating a factor graph
 * @author Frank Dellaert
 */

#include <stdarg.h>
#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>

#include <gtsam/linear/GaussianBayesNet.h>
#include <gtsam/linear/VectorValues.h>

#include <gtsam/inference/BayesNet-inl.h>

using namespace std;
using namespace gtsam;


// trick from some reading group
#define FOREACH_PAIR( KEY, VAL, COL) BOOST_FOREACH (boost::tie(KEY,VAL),COL) 
#define REVERSE_FOREACH_PAIR( KEY, VAL, COL) BOOST_REVERSE_FOREACH (boost::tie(KEY,VAL),COL)

namespace gtsam {

// Explicitly instantiate so we don't have to include everywhere
template class BayesNet<GaussianConditional>;

/* ************************************************************************* */
GaussianBayesNet scalarGaussian(Index key, double mu, double sigma) {
	GaussianBayesNet bn;
	GaussianConditional::shared_ptr
		conditional(new GaussianConditional(key, Vector_(1,mu)/sigma, eye(1)/sigma, ones(1)));
	bn.push_back(conditional);
	return bn;
}

/* ************************************************************************* */
GaussianBayesNet simpleGaussian(Index key, const Vector& mu, double sigma) {
	GaussianBayesNet bn;
	size_t n = mu.size();
	GaussianConditional::shared_ptr
		conditional(new GaussianConditional(key, mu/sigma, eye(n)/sigma, ones(n)));
	bn.push_back(conditional);
	return bn;
}

/* ************************************************************************* */
void push_front(GaussianBayesNet& bn, Index key, Vector d, Matrix R,
		Index name1, Matrix S, Vector sigmas) {
	GaussianConditional::shared_ptr cg(new GaussianConditional(key, d, R, name1, S, sigmas));
	bn.push_front(cg);
}

/* ************************************************************************* */
void push_front(GaussianBayesNet& bn, Index key, Vector d, Matrix R,
		Index name1, Matrix S, Index name2, Matrix T, Vector sigmas) {
	GaussianConditional::shared_ptr cg(new GaussianConditional(key, d, R, name1, S, name2, T, sigmas));
	bn.push_front(cg);
}

/* ************************************************************************* */
boost::shared_ptr<VectorValues> allocateVectorValues(const GaussianBayesNet& bn) {
  vector<size_t> dimensions(bn.size());
  Index var = 0;
  BOOST_FOREACH(const boost::shared_ptr<const GaussianConditional> conditional, bn) {
    dimensions[var++] = conditional->dim();
  }
  return boost::shared_ptr<VectorValues>(new VectorValues(dimensions));
}

/* ************************************************************************* */
VectorValues optimize(const GaussianBayesNet& bn) {
  return *optimize_(bn);
}

/* ************************************************************************* */
boost::shared_ptr<VectorValues> optimize_(const GaussianBayesNet& bn)
{
	// get the RHS as a VectorValues to initialize system
	boost::shared_ptr<VectorValues> result(new VectorValues(rhs(bn)));

  /** solve each node in turn in topological sort order (parents first)*/
	BOOST_REVERSE_FOREACH(GaussianConditional::shared_ptr cg, bn)
		cg->solveInPlace(*result); // solve and store solution in same step

	return result;
}

/* ************************************************************************* */
VectorValues backSubstitute(const GaussianBayesNet& bn, const VectorValues& y) {
	VectorValues x(y);
	backSubstituteInPlace(bn,x);
	return x;
}

/* ************************************************************************* */
// (R*x)./sigmas = y by solving x=inv(R)*(y.*sigmas)
void backSubstituteInPlace(const GaussianBayesNet& bn, VectorValues& y) {
	VectorValues& x = y;
	/** solve each node in turn in topological sort order (parents first)*/
	BOOST_REVERSE_FOREACH(const boost::shared_ptr<const GaussianConditional> cg, bn) {
		// i^th part of R*x=y, x=inv(R)*y
		// (Rii*xi + R_i*x(i+1:))./si = yi <-> xi = inv(Rii)*(yi.*si - R_i*x(i+1:))
		cg->solveInPlace(x);
	}
}

/* ************************************************************************* */
// gy=inv(L)*gx by solving L*gy=gx.
// gy=inv(R'*inv(Sigma))*gx
// gz'*R'=gx', gy = gz.*sigmas
VectorValues backSubstituteTranspose(const GaussianBayesNet& bn,
		const VectorValues& gx) {

	// Initialize gy from gx
	// TODO: used to insert zeros if gx did not have an entry for a variable in bn
	VectorValues gy = gx;

	// we loop from first-eliminated to last-eliminated
	// i^th part of L*gy=gx is done block-column by block-column of L
	BOOST_FOREACH(const boost::shared_ptr<const GaussianConditional> cg, bn)
		cg->solveTransposeInPlace(gy);

	// Scale gy
	BOOST_FOREACH(GaussianConditional::shared_ptr cg, bn)
		cg->scaleFrontalsBySigma(gy);

	return gy;
}

/* ************************************************************************* */  
pair<Matrix,Vector> matrix(const GaussianBayesNet& bn)  {

  // add the dimensions of all variables to get matrix dimension
  // and at the same time create a mapping from keys to indices
  size_t N=0; map<Index,size_t> mapping;
  BOOST_FOREACH(GaussianConditional::shared_ptr cg,bn) {
  	GaussianConditional::const_iterator it = cg->beginFrontals();
  	for (; it != cg->endFrontals(); ++it) {
  		mapping.insert(make_pair(*it,N));
  		N += cg->dim(it);
  	}
  }

  // create matrix and copy in values
  Matrix R = zeros(N,N);
  Vector d(N);
  Index key; size_t I;
  FOREACH_PAIR(key,I,mapping) {
    // find corresponding conditional
    boost::shared_ptr<const GaussianConditional> cg = bn[key];

    // get sigmas
    Vector sigmas = cg->get_sigmas();

    // get RHS and copy to d
    GaussianConditional::const_d_type d_ = cg->get_d();
    const size_t n = d_.size();
    for (size_t i=0;i<n;i++)
      d(I+i) = d_(i)/sigmas(i);

    // get leading R matrix and copy to R
    GaussianConditional::const_r_type R_ = cg->get_R();
    for (size_t i=0;i<n;i++)
      for(size_t j=0;j<n;j++)
      	R(I+i,I+j) = R_(i,j)/sigmas(i);

    // loop over S matrices and copy them into R
    GaussianConditional::const_iterator keyS = cg->beginParents();
    for (; keyS!=cg->endParents(); keyS++) {
      Matrix S = cg->get_S(keyS);                   // get S matrix
      const size_t m = S.rows(), n = S.cols(); // find S size
      const size_t J = mapping[*keyS];     // find column index
      for (size_t i=0;i<m;i++)
      	for(size_t j=0;j<n;j++)
      		R(I+i,J+j) = S(i,j)/sigmas(i);
    } // keyS

  } // keyI

  return make_pair(R,d);
}

/* ************************************************************************* */
VectorValues rhs(const GaussianBayesNet& bn) {
	boost::shared_ptr<VectorValues> result(allocateVectorValues(bn));
  BOOST_FOREACH(boost::shared_ptr<const GaussianConditional> cg,bn)
  	cg->rhs(*result);

  return *result;
}

/* ************************************************************************* */

} // namespace gtsam
