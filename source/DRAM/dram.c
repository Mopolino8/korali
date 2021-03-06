/*
 *  dram.c
 *  Pi4U
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <math.h>
#include <string.h>

#include <gsl/gsl_math.h>
#include <gsl/gsl_linalg.h>

#include "dram.h"

#include <fitfun.h>
#include <priors.h>
#include <myrand.h>


#ifndef min
	#define min(a,b) (a)<(b)?(a):(b)
#endif



#define BLANKS " \t\n"



void dram_finalize(){
}


void print_error(char *var)
{
	fprintf(stderr, "Invalid value for `%s'. Check the file dram.par\n", var);
	exit(EXIT_FAILURE);
}

void print_params(Params * par)
{
	/* Print all user-defined parameters */
	printf("\n***************************\n");
	printf("Running with parameters:\n");
	printf(" > Npar          =  %d\n", par->Npar);
	printf(" > Nsim          =  %d\n", par->Nsim);
	printf(" > DRscale       =  %g\n", par->DRscale);
	printf(" > AMinterv      =  %d\n", par->AMinterv);
	printf(" > seed          =  %d\n", par->seed);
	printf(" > Co            =  %g\n", par->qcov[0]);
	printf(" > Initial point =  %g ", par->par0[0] );
	for (int i = 1; i < par->Npar; i++)
		printf(" - %g ", par->par0[i]);
    printf("\n");
	printf(" > printfreq = %d\n", par->printfreq);
	printf("***************************\n");
	printf("\n");


    //printf("\npar.qcov:\n");
    //for (int i = 0; i < par->Npar; i++){
    //    for (int j = 0; j < par->Npar; j++)
    //        printf("%6.3f", par->qcov[i*par->Npar+j]);
    //    printf("\n");
    //}
    //printf("\n");
}

void read_par_file( const char * file, Params * par ){


	FILE *fp = fopen( file , "r");
	if (fp == NULL) {
		fprintf(stderr, "No dram.par file available.\n");
    	exit(EXIT_FAILURE);
    }


    size_t bufsize = 1024;
    char * buffer = (char *)malloc(bufsize * sizeof(char)); 


    while(  -1 != getline( &buffer, &bufsize, fp) ){
            
        char * pch = strtok (buffer, BLANKS );
        while( pch != NULL ){
            
            if( pch[0]=='#' || pch[0]=='\n' ) //go to the next line. 
                break;
            
            if( strcmp(pch,"Npar")==0 ){
                pch = strtok(NULL, BLANKS );
                par->Npar = atoi( pch );
			    par->AMscale = 2.4/sqrt(par->Npar);
                break;
            }

            if( strcmp(pch,"Nsim")==0 ){
                pch = strtok(NULL, BLANKS );
                par->Nsim = atoi( pch );
                break;
            }

            if( strcmp(pch,"seed")==0 ){
                pch = strtok(NULL, BLANKS );
                par->seed = atoi( pch );
                break;
            }

            if( strcmp(pch,"printfreq")==0 ){
                pch = strtok(NULL, BLANKS );
                par->printfreq = atoi( pch );
                break;
            }

            if( strcmp(pch,"verbose")==0 ){
                pch = strtok(NULL, BLANKS );
                par->verbose = atoi( pch );
                break;
            }

            if( strcmp(pch,"DRscale")==0 ){
                pch = strtok(NULL, BLANKS );
                par->DRscale = atof( pch );
                break;
            }

            if( strcmp(pch,"AMinterv")==0 ){
                pch = strtok(NULL, BLANKS );
                par->AMinterv = atof( pch );
                break;
            }

            if( strcmp(pch,"Co")==0 ){
                pch = strtok(NULL, BLANKS );
                double Co = atof( pch );
                
                if( par->Npar<=0 ){
                    printf("Define 'Npar' before 'Co' in %s. Exit...",file);
                    exit(EXIT_FAILURE);
                }
                
                par->qcov = (double *)malloc( sizeof(double)*par->Npar*par->Npar );

                for (int i = 0; i < par->Npar; i++)
                    for (int j = 0; j < par->Npar; j++)
                        if (i == j) 
                            par->qcov[i*par->Npar+j] = Co;
                        else 
                            par->qcov[i*par->Npar+j] = 0.0;
                break;
            }

            if( strcmp(pch,"par0")==0 ){
                
                if( par->Npar<=0 ){
                    printf("Define 'Npar' before 'par0' in %s. Exit...",file);
                    exit(EXIT_FAILURE);
                }

                par->par0 = (double *)malloc( sizeof(double)*par->Npar );

                //TODO: check if there are enough initial points
                for(int i=0; i<par->Npar; i++){ 
                    pch = strtok(NULL, BLANKS );
                    par->par0[i] = atof( pch );
                }
                break;
            }

            puts(buffer);
            printf("\nSomething went wrong while reading the parameter file %s. Exit...\n",file);
            exit(EXIT_FAILURE);

        }
    }

    fclose(fp);
    free(buffer);

}

void dram_init(Params * par){

    // default values
	par->Npar		= -1;
	par->Nsim		= -1;
	par->DRscale		= -1;

	par->AMinterv	= -1;
	par->AMscale		= -1;
	par->AMepsilon	= 1e-5; //val.


	par->sigma2 	= 1; 	// val.
	par->n0 		= -1;	// val.
	par->n 		= 0;	// val.

	par->qcov    = NULL;
	par->par0 	= NULL;
	par->lbounds = NULL;
	par->ubounds = NULL;

	strcpy(par->filename, "chain.txt");


	par->printfreq	= 2000;
	par->verbose		= 1;
    par->seed    	= 123987;


    read_par_file("dram.par", par);



	/* Check if user has provided all necessary parameters */
   	if (par->Npar == -1)		print_error((char*)"Npar");
   	if (par->Nsim == -1)		print_error((char*)"Nsim");
   	if (par->DRscale == -1)	print_error((char*)"DRscale");
	if (par->AMinterv == -1)	print_error((char*)"AMinterv");
	if (par->AMscale == -1)	print_error((char*)"Npar");
    if (par->par0 == NULL) 	print_error((char*)"Initial chain point");
    if (par->qcov == NULL) 	print_error((char*)"Covariance");

    
    par->lbounds = (double *)malloc(par->Npar*sizeof(double));
	par->ubounds = (double *)malloc(par->Npar*sizeof(double)); 
    for (int i = 0; i < par->Npar; i++){
        par->lbounds[i] = -1e16;
        par->ubounds[i] = +1e16;
    }
        


	int Ntmp;
	read_priors("priors.par", &par->prior, &Ntmp );
	if( Ntmp  != par->Npar ){
		printf("\nNumber of parameters in 'priors.par' is different than dram.par \n");
		exit(1);
	}


	gsl_rand_init(par->seed);


	if (par->verbose){
        print_priors( par->prior, par->Npar );
        print_params( par );
    }


}





double priorfun(double *x, int n, Density *d){
	
	return -2. * prior_log_pdf( d, n, x);
}


double ssfun(double *x, int n){

	double res = fitfun(x, n, NULL, NULL);
	return  -2. * res;
}


double norm(double *a, int n)
{
	double s = 0;
	for (int i = 0; i < n; i++)
		s += pow(a[i],2.0);

	return sqrt(s);
}


void covupd(double *x, int start, int end, int npar, double w, double *xcov, double *xmean, double *wsum){
	int n = end - start;
	int p = npar;

	if (n == 0) // nothing to update with
	{
		return;
	}
	
	if (start > 0)
	{ 

		double oldcov[p*p], oldmean[p], oldwsum;

		memcpy(oldcov, xcov, p*p*sizeof(double));
		memcpy(oldmean, xmean, p*sizeof(double));
		oldwsum = *wsum;


		for (int i=0; i<n; i++)
		{
			double xi[p];
			double xmeann[p];

			memcpy(xi, &x[i*p], p*sizeof(double));
			*wsum   = w;

			memcpy(xmeann, xi, p*sizeof(double));
			for (int j=0; j<p; j++)
			{
				xmean[j] = oldmean[j] + (*wsum)/(*wsum+oldwsum)*(xmeann[j]-oldmean[j]);
			}

			double fac = (*wsum)/(*wsum+oldwsum-1) * (oldwsum/(*wsum+oldwsum));
			double minus[p];
			double prod[p*p];
			for (int j=0; j<p; j++) minus[j]=xi[j]-oldmean[j];

			for (int j=0; j<p; j++) 
			for (int k=0; k<p; k++) 
				prod[j*p+k]=minus[j]*minus[k];


			for (int j=0; j<p; j++) 
			for (int k=0; k<p; k++) 
				xcov[j*p+k]=oldcov[j*p+k] + fac * (prod[j*p+k]-oldcov[j*p+k]);
    
			*wsum    = *wsum+oldwsum;
			memcpy(oldcov, xcov, p*p*sizeof(double));
			memcpy(oldmean, xmean, p*sizeof(double));
			oldwsum = *wsum;
		}
	}
	else
	{
		// no update

		*wsum  = w*n;
		memset(xmean, 0, p*sizeof(double));
		memset(xcov, 0, p*p*sizeof(double));

		for (int j=0; j<p; j++)
		{
			double s = 0;
			for (int i = 0; i < n; i++) 
				s += x[i*npar+j]*w;

			xmean[j] = s/(*wsum);
		}

		if (*wsum>1)
		{
			for (int i=0; i<p; i++)
			{
				for (int j=0; j<=i; j++)
				{
					double s = 0;
					for (int k = 0; k<n; k++)
					{
						s += (x[k*p+i]-xmean[i])*(x[k*p+j]-xmean[j])*w;
					}
					xcov[i*p+j] = s/(*wsum-1);

					if (i != j)
					{
						xcov[j*p+i] = xcov[i*p+j];
					}
				}
			}
		}
	}
}






void inv(double *Ainv, double *A, int n)
{
	int s;
	gsl_matrix *work = gsl_matrix_alloc(n,n), *winv = gsl_matrix_alloc(n,n);
	gsl_permutation *p = gsl_permutation_alloc(n);

	memcpy( work->data, A, n*n*sizeof(double) );
#if DEBUG
	gsl_matrix_fprintf(stdout, work, "%f");
#endif
	gsl_linalg_LU_decomp( work, p, &s );
	gsl_linalg_LU_invert( work, p, winv );
#if DEBUG
	gsl_matrix_fprintf(stdout, winv, "%f");
#endif
	memcpy( Ainv, winv->data, n*n*sizeof(double) );
	gsl_matrix_free( work );
	gsl_matrix_free( winv );
	gsl_permutation_free( p );
}

int chol(double *Achol, double *A, int n)
{
	gsl_matrix *work = gsl_matrix_alloc(n,n);
	memcpy(work->data, A, n*n*sizeof(double) );

	gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
	int status = gsl_linalg_cholesky_decomp(work);
	if (status == GSL_EDOM) {
		gsl_matrix_free(work);
		gsl_set_error_handler (old_handler); 
		return 1; // not positive-definite (singular)
	}

#if DEBUG
	gsl_matrix_fprintf(stdout, work, "%f");
#endif
	for (int i = 0; i < n; i++)
	for (int j = 0; j < n; j++)
		if (j<i)
			gsl_matrix_set(work, i, j, 0);

	memcpy( Achol, work->data, n*n*sizeof(double) );

#if DEBUG
	gsl_matrix_fprintf(stdout, work, "%f");
#endif

	gsl_set_error_handler (old_handler); 
	gsl_matrix_free(work);

	return 0; // all good
}


int check_bounds(double *x, double *lbounds, double *ubounds, int n)
{
	for (int i = 0; i < n; i++)
	{
		if ((x[i]<lbounds[i]) || (x[i]>ubounds[i]))
			return 1;
	}
	return 0;
}









void dram(Params * par)
{
/* Metropolis-Hastings MCMC with adaptive delayed rejection (DRAM)
 * Based on the MATLAB implementation of 
 * Haario, Laine, Mira and Saksman, 2006, doi:10.1007/s11222-006-9438-0
 */

	// parameters for the simulation model
	int npar	 = par->Npar;
	int nsimu	 = par->Nsim;
	double *par0 = par->par0;

	Density *prior = par->prior;

	double *lbounds	= par->lbounds;
	double *ubounds	= par->ubounds;
	if ((lbounds == NULL)||(ubounds == NULL))
	{
		printf("bounds not initialized\n");
		exit(1);
	}

	// parameters for DRAM
	int    adaptint = par->AMinterv;
	double drscale 	= par->DRscale;
	double adascale	= par->AMscale;
	double qcovadj 	= par->AMepsilon;
	int    n0 	= par->n0;
	double sigma2 	= par->sigma2;

#if 0
	// number of observations (needed for sigma2 update)
	int n = 0;
	if (n0 >= 0)
	{
		n = par->n;
	}
#endif

	double *qcov = par->qcov;

	// to DR or not to DR
	int dodr;
	if (drscale<=0)
		dodr=0; 
	else
		dodr=1;


	int printint  = par->printfreq;
	int verbosity = par->verbose;

	double R[npar*npar];
	double R2[npar*npar];
	double iR[npar*npar];

	chol(R, qcov, npar); // Cholesky factor of proposal covariance
	if (dodr)
	{
		for (int i = 0; i < npar*npar; i++) R2[i] = R[i]/drscale; // second proposal for DR try
		inv(iR, R, npar);
	}

	double *chain 	 = (double *)calloc(1, nsimu*npar*sizeof(double));
	double *loglike  = (double *)calloc(1, nsimu*sizeof(double));
	double *logprior = (double *)calloc(1, nsimu*sizeof(double));

	double s20 = 0;
	double *s2chain;
	if (n0>=0)
	{
		s2chain = (double *)calloc(1, nsimu*sizeof(double)); // the sigma2 chain
		s20 = sigma2;
	}
	else
	{
		s2chain = NULL;
	}

	double oldpar[npar];
	memcpy(oldpar, par0, npar*sizeof(double)); // first row of the chain

	double oldss    = ssfun(oldpar, npar); // first sum-of-squares
	double oldprior = priorfun( oldpar, npar, prior );

#if DEBUG
	printf("oldss = %f\n", oldss);
#endif

	int acce = 1; //  how many accepted moves

	memcpy( &chain[0*npar], oldpar, npar*sizeof(double) ); 
	loglike[0]  = oldss;
	logprior[0] = oldprior;

	if (s20>0)
		s2chain[0] = sigma2;


	// covariance update uses these to store previous values
	double chaincov[npar*npar];
	double chainmean[npar];
	double wsum = 0;
	int lasti = 0;

	/* THE CHAIN LOOP */
	int isimu;
	for (isimu=1; isimu<nsimu; isimu++)
	{
		if ((isimu+1)%printint == 0) 
		{
			printf("isim=%d, %d%% done, accepted: %d%%\n",
			  isimu,(int)((float)(isimu+1)/nsimu*100),(int)(((float)acce/isimu)*100));
		}
  
		double newpar[npar];

		for (int i = 0; i < npar; i++)
		{
			newpar[i] = oldpar[i];
			for (int j = 0; j < npar; j++)
				newpar[i] += normalrand(0,1)*R[i*npar+j]; // a new proposal 
		}

		int accept;
		accept = 0;
		// check bounds
		double newss, newprior, alpha12;
		if (check_bounds(newpar, lbounds, ubounds, npar))
		{
			newss = DBL_MAX; //+Inf;
			newprior = 0;
			alpha12 = 0;
		}
		else{ // inside bounds, check if accepted
			newss  = ssfun(newpar,npar);
			newprior = priorfun(newpar,npar,prior);
			alpha12 = min(1,exp(-0.5*(newss-oldss)/sigma2-0.5*(newprior-oldprior)));
			double r = uniformrand(0,1);
			if (r < alpha12){ // we accept
				accept   = 1;
				acce     = acce+1;
				#if DEBUG
				printf("accepting (%f<%f): from [%f %f]=(%f) to [%f %f]=(%f)\n", r, alpha12, oldpar[0], oldpar[1], oldss, newpar[0], newpar[1], newss); 
				#endif
				memcpy(oldpar, newpar, npar*sizeof(double));
				oldss    = newss;
				oldprior = newprior;
			}
			else{
				#if DEBUG
				printf("not accepting (%f>%f): from [%f %f]=(%f) to [%f %f]=(%f)\n", r, alpha12, oldpar[0], oldpar[1], oldss, newpar[0], newpar[1],newss);
				#endif
			}
		}

		if (accept == 0 && dodr){ // we reject, but make a new try (DR)
			// a new try
			double newpar2[npar];
			for (int i = 0; i < npar; i++){
				newpar2[i] = oldpar[i];
				for (int j = 0; j < npar; j++)
					newpar2[i] += normalrand(0,1)*R2[i*npar+j]; 
			}

			double newss2, newprior2;
			if (check_bounds(newpar2, lbounds, ubounds, npar)){
				newss2 = DBL_MAX; //+Inf;
				newprior2 = 0;
			}
			else{ // inside bounds
				newss2    = ssfun(newpar2,npar);
				newprior2 = priorfun(newpar2,npar,prior);
				double alpha32 = min(1,exp(-0.5*(newss-newss2)/sigma2 -0.5*(newprior-newprior2)));
				double l2 = exp(-0.5*(newss2-oldss)/sigma2 - 0.5*(newprior2-oldprior));
				//double q1 = exp(-0.5*(norm((newpar2-newpar)*iR)^2-norm((oldpar-newpar)*iR)^2));
				double q1;
				{
					double term1[npar], minus1[npar];
					for (int i = 0; i < npar; i++) minus1[i] = newpar2[i]-newpar[i];
					for (int i = 0; i < npar; i++) 
					{
						for (int j = 0; j < npar; j++)
							term1[i] = minus1[i]*iR[i*npar+j];
					}

					double term2[npar], minus2[npar];
					for (int i = 0; i < npar; i++) minus2[i] = oldpar[i]-newpar[i];
					for (int i = 0; i < npar; i++) 
					{
						for (int j = 0; j < npar; j++)
							term2[i] = minus2[i]*iR[i*npar+j];
					}
					double norm1 = norm(term1, npar);
					double norm2 = norm(term2, npar);
					q1 = exp(-0.5*(norm1*norm1-norm2*norm2));
				}

				double alpha13 = l2*q1*(1-alpha32)/(1-alpha12);
				if (uniformrand(0,1) < alpha13){ // we accept
					accept = 1;
					acce     = acce+1;
					memcpy(oldpar, newpar2, npar*sizeof(double)); // oldpar = newpar2;
					oldss    = newss2;
					oldprior = newprior2;
				}
			}
 		}
  
		memcpy(&chain[isimu*npar], oldpar, npar*sizeof(double));
		loglike[isimu]  = oldss;
		logprior[isimu] = oldprior;


		// update the error variance sigma2
		// TODO: what is going on here?
		if (s20 > 0){
			//sigma2  = 1./gammar_mt(1,1,(n0+n)./2,2./(n0*s20+oldss));
			//s2chain(isimu,:) = sigma2;
		}
  
		if (adaptint>0 && ((isimu+1)%adaptint == 0)){
			// adapt the proposal covariances
			if (verbosity) printf("adapting\n");

			// update covariance and mean of the chain
			covupd(&chain[lasti*npar], lasti, isimu, npar, 1, chaincov, chainmean, &wsum);
			lasti = isimu;

			double chaincov_tmp[npar*npar];
			for (int i = 0; i < npar; i++)
			for (int j = 0; j < npar; j++)
				if (i == j)
					chaincov_tmp[i*npar+j] = chaincov[i*npar+j] + qcovadj;
				else
					chaincov_tmp[i*npar+j] = chaincov[i*npar+j];

			
			double Ra[npar*npar];
			int is = chol(Ra, chaincov_tmp, npar);

			if(is){ // singular cmat
				printf("Warning cmat singular, not adapting\n");
			}
			else{
				for (int i = 0; i < npar; i++)
				for (int j = 0; j < npar; j++)
					R[i*npar+j] = Ra[i*npar+j]*adascale;

				if (dodr){  
					for (int i = 0; i < npar*npar; i++) R2[i] = R[i]/drscale; // second proposal for DR try
					inv(iR, R, npar);
				}
			}
		}
	}


	printf("acceptance = %d\n", (int)((100.0*acce)/nsimu));
	FILE *fp = fopen(par->filename, "w");
	for (int isimu=0; isimu<nsimu; isimu++){

		for( int j=0; j<npar; j++)
			fprintf(fp, "%lf \t ", chain[isimu*npar+j] );

		fprintf(fp, "%lf \t ", -loglike[isimu]/2. );
		fprintf(fp, "%lf \n ", -logprior[isimu]/2. );
	}
	fclose(fp);





	//---------------------------------------------------------------------------------
	//	TODO: delete this part?	
	//
	//printf("lasti: %d\n", lasti);
	//printf("isim: %d\n", isimu);

	covupd(&chain[lasti*npar], lasti, isimu, npar, 1, chaincov, chainmean, &wsum);

	//for (int i = 0; i < npar; i++)
	//	printf("chainmean[%d]=%.4f\n", i, chainmean[i]);

	//for (int i = 0; i < npar; i++)
	//for (int j = 0; j < npar; j++)
	//	printf("chaincov[%d,%d]=%.4f\n", i, j, chaincov[i*npar+j]);

	//printf("wsum = %f\n", wsum);
	//---------------------------------------------------------------------------------
}
