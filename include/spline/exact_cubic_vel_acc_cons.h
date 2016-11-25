/**
* \file exact_cubic.h
* \brief class allowing to create an Exact cubic spline.
* \author Steve T.
* \version 0.1
* \date 06/17/2013
*
* This file contains definitions for the ExactCubic class.
* Given a set of waypoints (x_i*) and timestep (t_i), it provides the unique set of
* cubic splines fulfulling those 4 restrictions :
* - x_i(t_i) = x_i* ; this means that the curve passes trough each waypoint
* - x_i(t_i+1) = x_i+1* ;
* - its derivative is continous at t_i+1
* - its acceleration is continous at t_i+1
* more details in paper "Task-Space Trajectories via Cubic Spline Optimization"
* By J. Zico Kolter and Andrew Y.ng (ICRA 2009)
*/


#ifndef _CLASS_CUBICZEROVELACC
#define _CLASS_CUBICZEROVELACC

#include "exact_cubic.h"

#include "MathDefs.h"

#include <functional>
#include <vector>

namespace spline
{
/// \class cubic_zero_vel.
/// \brief Represents a set of cubic splines defining a continuous function 
/// crossing each of the waypoint given in its initialization. Additional constraints
/// are used to increase the order of the last and first splines, to start and finish
/// trajectory with zero velocity and acceleration. Thus the first and last splines
///
///
template<typename Time= double, typename Numeric=Time, std::size_t Dim=3, bool Safe=false,
         typename Point= Eigen::Matrix<Numeric, Dim, 1>,
         typename T_Point =std::vector<Point,Eigen::aligned_allocator<Point> > >
struct cubic_zero_vel : public curve_abc<Time, Numeric, Dim, Safe, Point>
{
    typedef Point 	point_t;
    typedef T_Point t_point_t;
    typedef Eigen::Matrix<Numeric, Eigen::Dynamic, Eigen::Dynamic> MatrixX;
    typedef Time 	time_t;
    typedef Numeric	num_t;
    typedef spline_curve<time_t, Numeric, Dim, Safe, point_t, t_point_t> spline_t;
    typedef curve_abc<Time, Numeric, Dim, Safe, Point> curve_abc_t;
    typedef typename std::vector<spline_t> t_spline_t;
    typedef typename t_spline_t::iterator it_spline_t;
    typedef typename t_spline_t::const_iterator cit_spline_t;

	/* Constructors - destructors */
	public:
	///\brief Constructor
	///\param wayPointsBegin : an iterator pointing to the first element of a waypoint container
	///\param wayPointsEns   : an iterator pointing to the end           of a waypoint container
	template<typename In>
    cubic_zero_vel(In wayPointsBegin, In wayPointsEnd)
        : curve_abc_t(), subSplines_(computeWayPoints<In>(wayPointsBegin, wayPointsEnd)) {}

	///\brief Destructor
    ~cubic_zero_vel(){}

    protected:
    template<typename In>
    t_spline_t computeWayPoints(In wayPointsBegin, In wayPointsEnd) const
    {
        std::size_t const size(std::distance(wayPointsBegin, wayPointsEnd));
        if(Safe && size < 1)
        {
            throw; // TODO
        }
        t_spline_t subSplines; subSplines.reserve(size);

        // refer to the paper to understand all this.
        MatrixX h1 = MatrixX::Zero(size, size);
        MatrixX h2 = MatrixX::Zero(size, size);
        MatrixX h3 = MatrixX::Zero(size, size);
        MatrixX h4 = MatrixX::Zero(size, size);
        MatrixX h5 = MatrixX::Zero(size, size);
        MatrixX h6 = MatrixX::Zero(size, size);

        MatrixX a =  MatrixX::Zero(size, Dim);
        MatrixX b =  MatrixX::Zero(size, Dim);
        MatrixX c =  MatrixX::Zero(size, Dim);
        MatrixX d =  MatrixX::Zero(size, Dim);
        MatrixX x =  MatrixX::Zero(size, Dim);


        In it(wayPointsBegin), next(wayPointsBegin);
        ++next;
        for(std::size_t i(0); next != wayPointsEnd; ++next, ++it, ++i)
        {
            num_t const dTi((*next).first  - (*it).first);
            num_t const dTi_sqr(dTi * dTi);
            num_t const dTi_cube(dTi_sqr * dTi);
            // filling matrices values
            h3(i,i)   = -3 / dTi_sqr;
            h3(i,i+1) =  3 / dTi_sqr;
            h4(i,i)   = -2 / dTi;
            h4(i,i+1) = -1 / dTi;
            num_t coeff_h5(-2);
            if(i == 0)
            {
                coeff_h5 = 1;
            }
            h5(i,i)   = -coeff_h5 / dTi_cube;
            h5(i,i+1) =  coeff_h5 / dTi_cube;
            h6(i,i)   =  1 / dTi_sqr;
            h6(i,i+1) =  1 / dTi_sqr;
            if( i+2 < size)
            {
                In it2(next); ++ it2;
                num_t const dTi_1((*it2).first - (*next).first);
                num_t const dTi_1sqr(dTi_1 * dTi_1);
                // this can be optimized but let's focus on clarity as long as not needed
                h1(i+1, i)   =  2 / dTi;
                h1(i+1, i+1) =  4 / dTi + 4 / dTi_1;
                h1(i+1, i+2) =  2 / dTi_1;
                h2(i+1, i)   = -6 / dTi_sqr;
                h2(i+1, i+1) = (6 / dTi_1sqr) - (6 / dTi_sqr);
                h2(i+1, i+2) =  6 / dTi_1sqr;
            }
            x.row(i)= (*it).second.transpose();
        }
        // adding last x
        x.row(size-1)= (*it).second.transpose();
        a= x;
        h1(0,0)= 1;
        h3(0,0)= 0;h3(0,1)= 0;
        h4(0,0)= 0;h4(0,1)= 0;
        h6(0,0)= 0;h6(0,1)= 0;
        PseudoInverse(h1);
        b = h1 * h2 * x; //h1 * b = h2 * x => b = (h1)^-1 * h2 * x
        c = h3 * x + h4 * b;
        d = h5 * x + h6 * b;
        it= wayPointsBegin, next=wayPointsBegin; ++ next;
        for(int i=0; next != wayPointsEnd; ++i, ++it, ++next)
        {
            subSplines.push_back(
                create_cubic<Time,Numeric,Dim,Safe,Point,T_Point>(a.row(i), b.row(i), c.row(i), d.row(i),(*it).first, (*next).first));
        }
        subSplines.push_back(
                create_cubic<Time,Numeric,Dim,Safe,Point,T_Point>(a.row(size-1), b.row(size-1), c.row(size-1), d.row(size-1), (*it).first, (*it).first));
        return subSplines;
    }


	private:
    cubic_zero_vel(const cubic_zero_vel&);
    cubic_zero_vel& operator=(const cubic_zero_vel&);
    /* Constructors - destructors */

    /*Operations*/
    public:
    ///  \brief Evaluation of the cubic spline at time t.
    ///  \param t : the time when to evaluate the spine
    ///  \param return : the value x(t)
    virtual point_t operator()(time_t t) const
    {
        if(Safe && (t < subSplines_.front().min() || t > subSplines_.back().max())){throw std::out_of_range("TODO");}
        for(cit_spline_t it = subSplines_.begin(); it != subSplines_.end(); ++ it)
        {
            if(t >= (it->min()) && t <= (it->max()))
            {
                return it->operator()(t);
            }
        }
    }
    /*Operations*/

    /*Helpers*/
    public:
    num_t virtual min() const{return subSplines_.front().min();}
    num_t virtual max() const{return subSplines_.back().max();}
    /*Helpers*/

    /*Attributes*/
    public:
    const t_spline_t subSplines_;
    /*Attributes*/
};
}
#endif //_CLASS_CUBICZEROVELACC
