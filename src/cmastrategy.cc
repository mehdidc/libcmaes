/**
 * CMA-ES, Covariance Matrix Adaptation Evolution Strategy
 * Copyright (c) 2014 Inria
 * Author: Emmanuel Benazera <emmanuel.benazera@lri.fr>
 *
 * This file is part of libcmaes.
 *
 * libcmaes is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libcmaes is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libcmaes.  If not, see <http://www.gnu.org/licenses/>.
 */

//#define NDEBUG 1

#include "cmastrategy.h"
#include "opti_err.h"
#include <glog/logging.h>
#include <iostream>

namespace libcmaes
{

  template <class TGenoPheno> using eostrat = ESOStrategy<CMAParameters<TGenoPheno>,CMASolutions,CMAStopCriteria<TGenoPheno> >;
  
  template <class TCovarianceUpdate, class TGenoPheno>
  ProgressFunc<CMAParameters<TGenoPheno>,CMASolutions> CMAStrategy<TCovarianceUpdate,TGenoPheno>::_defaultPFunc = [](const CMAParameters<TGenoPheno> &cmaparams, const CMASolutions &cmasols)
  {
    LOG_IF(INFO,!cmaparams._quiet) << "iter=" << cmasols._niter << " / evals=" << cmaparams._lambda * cmasols._niter << " / f-value=" << cmasols._best_candidates_hist.back()._fvalue <<  " / sigma=" << cmasols._sigma << (cmaparams._lazy_update && cmasols._updated_eigen ? " / cupdate="+std::to_string(cmasols._updated_eigen) : "") << " / kl=" << cmasols._kl << std::endl;
    return 0;
  };
  
  template <class TCovarianceUpdate, class TGenoPheno>
  CMAStrategy<TCovarianceUpdate,TGenoPheno>::CMAStrategy(FitFunc &func,
					      CMAParameters<TGenoPheno> &parameters)
    :ESOStrategy<CMAParameters<TGenoPheno>,CMASolutions,CMAStopCriteria<TGenoPheno> >(func,parameters)
  {
    eostrat<TGenoPheno>::_pfunc = _defaultPFunc;
    _esolver = EigenMultivariateNormal<double>(false,eostrat<TGenoPheno>::_parameters._seed); // seeding the multivariate normal generator.
    LOG_IF(INFO,!eostrat<TGenoPheno>::_parameters._quiet) << "CMA-ES / dim=" << eostrat<TGenoPheno>::_parameters._dim << " / lambda=" << eostrat<TGenoPheno>::_parameters._lambda << " / mu=" << eostrat<TGenoPheno>::_parameters._mu << " / mueff=" << eostrat<TGenoPheno>::_parameters._muw << " / c1=" << eostrat<TGenoPheno>::_parameters._c1 << " / cmu=" << eostrat<TGenoPheno>::_parameters._cmu << " / lazy_update=" << eostrat<TGenoPheno>::_parameters._lazy_update << std::endl;
    if (!eostrat<TGenoPheno>::_parameters._fplot.empty())
      _fplotstream.open(eostrat<TGenoPheno>::_parameters._fplot);
  }

  template <class TCovarianceUpdate, class TGenoPheno>
  CMAStrategy<TCovarianceUpdate,TGenoPheno>::~CMAStrategy()
  {
    if (!eostrat<TGenoPheno>::_parameters._fplot.empty())
      _fplotstream.close();
  }
  
  template <class TCovarianceUpdate, class TGenoPheno>
  dMat CMAStrategy<TCovarianceUpdate,TGenoPheno>::ask()
  {
    // compute eigenvalues and eigenvectors.
    eostrat<TGenoPheno>::_solutions._updated_eigen = false;
    if (eostrat<TGenoPheno>::_niter == 0 || !eostrat<TGenoPheno>::_parameters._lazy_update
	|| eostrat<TGenoPheno>::_niter - eostrat<TGenoPheno>::_solutions._eigeniter > eostrat<TGenoPheno>::_parameters._lazy_value)
      {
	eostrat<TGenoPheno>::_solutions._eigeniter = eostrat<TGenoPheno>::_niter;
	_esolver.setMean(eostrat<TGenoPheno>::_solutions._xmean);
	_esolver.setCovar(eostrat<TGenoPheno>::_solutions._cov);
	eostrat<TGenoPheno>::_solutions._updated_eigen = true;
      }
    
    // sample for multivariate normal distribution.
    dMat pop = _esolver.samples(eostrat<TGenoPheno>::_parameters._lambda,eostrat<TGenoPheno>::_solutions._sigma); // Eq (1).
    
    //TODO: rescale to function space as needed.

    //debug
    /*DLOG(INFO) << "ask: produced " << pop.cols() << " candidates\n";
      std::cerr << pop << std::endl;*/
    //debug
    
    return pop;
  }
  
  template <class TCovarianceUpdate, class TGenoPheno>
  void CMAStrategy<TCovarianceUpdate,TGenoPheno>::tell()
  {
    //debug
    //DLOG(INFO) << "tell()\n";
    //debug
    
    // sort candidates.
    eostrat<TGenoPheno>::_solutions.sort_candidates();

    //TODO: test for flat values (same value almost everywhere).

    //TODO: update function value history, as needed.
    eostrat<TGenoPheno>::_solutions.update_best_candidates();

    //TODO: update best value, as needed.

    // CMA-ES update, depends on the selected 'flavor'.
    TCovarianceUpdate::update(eostrat<TGenoPheno>::_parameters,_esolver,eostrat<TGenoPheno>::_solutions);
    
    // other stuff.
    if (eostrat<TGenoPheno>::_niter > 0 && eostrat<TGenoPheno>::_parameters._kl)
      compute_kl();
    eostrat<TGenoPheno>::_solutions.update_eigenv(_esolver._eigenSolver.eigenvalues(),
			     _esolver._eigenSolver.eigenvectors());
    eostrat<TGenoPheno>::_solutions._niter = eostrat<TGenoPheno>::_niter;
    eostrat<TGenoPheno>::_solutions._covold = eostrat<TGenoPheno>::_solutions._cov; //TODO: in solutions.
    eostrat<TGenoPheno>::_solutions._xmeanold = eostrat<TGenoPheno>::_solutions._xmean;
    eostrat<TGenoPheno>::_solutions._sigmaold = eostrat<TGenoPheno>::_solutions._sigma;
  }

  template <class TCovarianceUpdate, class TGenoPheno>
  bool CMAStrategy<TCovarianceUpdate,TGenoPheno>::stop()
  {
    if (eostrat<TGenoPheno>::_solutions._run_status < 0) // an error occured, most likely out of memory at cov matrix creation.
      return true;
    
    if (eostrat<TGenoPheno>::_niter == 0)
      return false;
    
    if (eostrat<TGenoPheno>::_pfunc(eostrat<TGenoPheno>::_parameters,eostrat<TGenoPheno>::_solutions)) // progress function.
      return true; // end on progress function internal termination, possibly custom.
    
    if (!eostrat<TGenoPheno>::_parameters._fplot.empty())
      plot();
    
    if ((eostrat<TGenoPheno>::_parameters._max_iter > 0 && eostrat<TGenoPheno>::_niter >= eostrat<TGenoPheno>::_parameters._max_iter)
	|| (eostrat<TGenoPheno>::_solutions._run_status = _stopcriteria.stop(eostrat<TGenoPheno>::_parameters,eostrat<TGenoPheno>::_solutions)) != 0)
      return true;
    else return false;
  }

  template <class TCovarianceUpdate, class TGenoPheno>
  int CMAStrategy<TCovarianceUpdate,TGenoPheno>::optimize()
  {
    //debug
    //DLOG(INFO) << "optimize()\n";
    //debug
    
    while(!stop())
      {
	dMat candidates = ask();
	this->eval(eostrat<TGenoPheno>::_parameters._gp.pheno(candidates));
	tell();
	eostrat<TGenoPheno>::_niter++;
      }
    if (eostrat<TGenoPheno>::_solutions._run_status >= 0)
      return OPTI_SUCCESS;
    else return OPTI_ERR_TERMINATION; // exact termination code is in eostrat<TGenoPheno>::_solutions._run_status.
  }

  template <class TCovarianceUpdate, class TGenoPheno>
  void CMAStrategy<TCovarianceUpdate,TGenoPheno>::plot()
  {
    static std::string sep = " ";
    _fplotstream << fabs(eostrat<TGenoPheno>::_solutions._best_candidates_hist.back()._fvalue) << sep
		 << eostrat<TGenoPheno>::_nevals << sep << eostrat<TGenoPheno>::_solutions._sigma << sep << sqrt(eostrat<TGenoPheno>::_solutions._max_eigenv/eostrat<TGenoPheno>::_solutions._min_eigenv) << sep;
    _fplotstream << _esolver._eigenSolver.eigenvalues().transpose() << sep; // eigenvalues
    _fplotstream << eostrat<TGenoPheno>::_solutions._cov.sqrt().diagonal().transpose() << sep; // max deviation in all main axes
    _fplotstream << eostrat<TGenoPheno>::_solutions._xmean.transpose() << sep;
    _fplotstream << eostrat<TGenoPheno>::_solutions._kl << sep << eostrat<TGenoPheno>::_solutions._kl_approx_det << sep << eostrat<TGenoPheno>::_solutions._kl_approx_trace_det << sep << eostrat<TGenoPheno>::_solutions._sigma_divergence << sep << eostrat<TGenoPheno>::_solutions._maha;
    _fplotstream << std::endl;
  }

  template <class TCovarianceUpdate, class TGenoPheno>
  void CMAStrategy<TCovarianceUpdate,TGenoPheno>::compute_kl()
  {
    dMat covinv = (pow(eostrat<TGenoPheno>::_solutions._sigma,2) * eostrat<TGenoPheno>::_solutions._cov).inverse();
    double tracenosig = (eostrat<TGenoPheno>::_solutions._cov.inverse()*eostrat<TGenoPheno>::_solutions._covold).trace();
    double trace = (covinv*(pow(eostrat<TGenoPheno>::_solutions._sigmaold,2) * eostrat<TGenoPheno>::_solutions._covold)).trace();
    dVec xmeandiff = eostrat<TGenoPheno>::_solutions._xmean - eostrat<TGenoPheno>::_solutions._xmeanold;
    double maha = xmeandiff.transpose()*covinv*xmeandiff;
    eostrat<TGenoPheno>::_solutions._maha = xmeandiff.transpose()*eostrat<TGenoPheno>::_solutions._cov.inverse()*xmeandiff;
    double det0 = (pow(eostrat<TGenoPheno>::_solutions._sigmaold,2)*eostrat<TGenoPheno>::_solutions._covold).determinant();
    double det1 = (pow(eostrat<TGenoPheno>::_solutions._sigma,2)*eostrat<TGenoPheno>::_solutions._cov).determinant();
    double det0nosig = eostrat<TGenoPheno>::_solutions._covold.determinant();
    double det1nosig = eostrat<TGenoPheno>::_solutions._cov.determinant();
    double det = std::log(det0/det1);

    //debug
    //std::cerr << "trace=" << trace << " / maha=" << maha << " / det=" << det << std::endl;
    //debug
    
    eostrat<TGenoPheno>::_solutions._kl = 0.5 * (trace + maha - eostrat<TGenoPheno>::_parameters._dim - det);
    eostrat<TGenoPheno>::_solutions._kl_approx_det = 0.5 * (trace + maha - eostrat<TGenoPheno>::_parameters._dim);
    eostrat<TGenoPheno>::_solutions._kl_approx_trace_det = 0.5 * (tracenosig - eostrat<TGenoPheno>::_parameters._dim - std::log(det0nosig/det1nosig));
    eostrat<TGenoPheno>::_solutions._sigma_divergence = 0.5 * ((eostrat<TGenoPheno>::_solutions._sigma*eostrat<TGenoPheno>::_solutions._sigma)-(eostrat<TGenoPheno>::_solutions._sigmaold*eostrat<TGenoPheno>::_solutions._sigmaold));
  }
  
  template class CMAStrategy<CovarianceUpdate,GenoPheno<NoBoundStrategy>>;
  template class CMAStrategy<ACovarianceUpdate,GenoPheno<NoBoundStrategy>>;
  template class CMAStrategy<CovarianceUpdate,GenoPheno<pwqBoundStrategy>>;
  template class CMAStrategy<ACovarianceUpdate,GenoPheno<pwqBoundStrategy>>;
}
