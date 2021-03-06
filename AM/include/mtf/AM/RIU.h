#ifndef MTF_RIU_H
#define MTF_RIU_H

#include "AppearanceModel.h"

_MTF_BEGIN_NAMESPACE

struct RIUParams : AMParams{

	//! decides whether logging data will be printed for debugging purposes; 
	//! only matters if logging is enabled at compile time
	bool debug_mode;
	//! value constructor
	RIUParams(const AMParams *am_params,
		 bool _debug_mode);
	//! default/copy constructor
	RIUParams(const RIUParams *params = nullptr);
};

struct RIUDist : AMDist{
	typedef double ElementType;
	typedef double ResultType;
	RIUDist(const string &_name, const bool _dist_from_likelihood,
		const double _likelihood_alpha, const unsigned int _patch_size);
	double operator()(const double* a, const double* b,
		size_t size, double worst_dist = -1) const override;
private:
	const bool dist_from_likelihood;
	const double likelihood_alpha;
	const unsigned int patch_size;
};

//! Ratio Image Uniformity
class RIU : public AppearanceModel{
public:
	typedef RIUParams ParamType;
	typedef RIUDist DistType;

	RIU(const ParamType *riu_params = nullptr, const int _n_channels = 1);

	double getLikelihood() const override;

	bool isSymmetrical() const override{ return false; }

	//-------------------------------initialize functions------------------------------------//
	void initializeSimilarity() override;
	void initializeGrad() override;
	void initializeHess() override{}

	//-------------------------------update functions------------------------------------//
	void updateSimilarity(bool prereq_only = true) override;
	void updateInitGrad() override;
	void updateCurrGrad() override;

	//-------------------------------interfacing functions------------------------------------//
	void cmptInitHessian(MatrixXd &init_hessian, const MatrixXd &init_pix_jacobian) override;
	void cmptInitHessian(MatrixXd &init_hessian, const MatrixXd &init_pix_jacobian,
		const MatrixXd &init_pix_hessian) override;
	void cmptCurrHessian(MatrixXd &curr_hessian, const MatrixXd &curr_pix_jacobian) override;
	void cmptCurrHessian(MatrixXd &curr_hessian, const MatrixXd &curr_pix_jacobian,
		const MatrixXd &curr_pix_hessian) override;
		void cmptSelfHessian(MatrixXd &self_hessian, const MatrixXd &curr_pix_jacobian) override;
	void cmptSelfHessian(MatrixXd &self_hessian, const MatrixXd &curr_pix_jacobian,
		const MatrixXd &curr_pix_hessian) override;

	// -------------------- distance feature functions -------------------- //
	VectorXd curr_feat_vec;
	const DistType* getDistFunc() override{
		return new DistType(name, params.dist_from_likelihood, 
			params.likelihood_alpha, patch_size);
	}
	unsigned int getDistFeatSize() override{ return patch_size + 1; }
	void initializeDistFeat() override{
		curr_feat_vec.resize(patch_size + 1);
	}
	void updateDistFeat(double* feat_addr) override;
	const double* getDistFeat() override{ return curr_feat_vec.data(); }
	void updateDistFeat() override{		
		updateDistFeat(curr_feat_vec.data());
		curr_feat_vec[0] = 1;
	}

protected:
	ParamType params;

	double N_inv;
	VectorXd r;
	VectorXd r_cntr;
	VectorXd df_dr;
	VectorXd dr_dIt, dr_dI0;
	double r_mean, r_var;
	double r_mean_inv;
};

_MTF_END_NAMESPACE

#endif