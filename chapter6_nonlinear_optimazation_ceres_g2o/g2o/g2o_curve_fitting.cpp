#include<iostream>
#include<g2o/core/base_vertex.h>
#include<g2o/core/base_unary_edge.h>
#include<g2o/core/block_solver.h>
#include<g2o/core/optimization_algorithm_levenberg.h>
#include<g2o/core/optimization_algorithm_gauss_newton.h>
#include<g2o/core/optimization_algorithm_dogleg.h>
#include<g2o/solvers/dense/linear_solver_dense.h>

#include<Eigen/Core>
#include<opencv2/core/core.hpp>
#include<cmath>
#include<chrono>
using namespace std;

// define the vertex of the graph
// dim of the to be optimizated variable, data type
class CurveFittingVertex: public g2o::BaseVertex<3, Eigen::Vector3d>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    virtual void setToOriginImpl() //reset
    {
        _estimate << 0,0,0;
    }
    virtual void oplusImpl(const double * update)
    {
        _estimate += Eigen::Vector3d(update);
    }
    virtual bool read( istream& in) {}
    virtual bool write( ostream& out) const {}
};
// define the edge of the graph
// dim of the observation, data type,vertex (variable to be optimizated)
class CurveFittingEdge:public g2o::BaseUnaryEdge<1, double, CurveFittingVertex>
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    CurveFittingEdge(double x): BaseUnaryEdge(), _x(x){}
    //to compute the error between the measurement and the values drawm from the vertex
    void computeError()
    {
        const CurveFittingVertex* v=static_cast<const CurveFittingVertex*> (_vertices[0]);
        const Eigen::Vector3d abc = v->estimate();
        _error(0,0) = _measurement - std::exp(abc(0,0)*_x*_x + abc(1,0)*_x + abc(2,0));
    }    
    virtual bool read( istream& in) {}
    virtual bool write( ostream& out) const {}
public:
     double _x; // y=measurement
};

/*
- _measurement：存储观测值
-_error：存储computeError() 函数计算的误差
-_vertices[]：存储顶点信息，比如二元边的话，_vertices[] 的大小为2，存储顺序和调用setVertex(int, vertex) 是设定的int 有关（0 或1）
-setId(int)：来定义边的编号（决定了在H矩阵中的位置）
-setMeasurement(type) 函数来定义观测值
-setVertex(int, vertex) 来定义顶点
-setInformation() 来定义协方差矩阵的逆*/

int main(int agrc, char** argv){
    double a=1.0, b=2.0, c=1.0;
    int N=100;
    double w_sigma=1.0;
    cv::RNG rng;
    double abc[3] = {0,0,0};

    vector<double>x_data, y_data;
    cout<<"generating data"<<endl;
    for(int i=0; i<N; i++) 
    {
        double x=i/100.0;
        x_data.push_back(x);
        y_data.push_back(exp(a*x*x + b*x +c)+ rng.gaussian(w_sigma));
        cout<<x_data[i]<<" "<<y_data[i]<<endl;
    }

    // set the parameter of the graph and the g2o
    // set the matrix block, the dim of the optimization is 3, dim of error is 1
    typedef g2o::BlockSolver<g2o::BlockSolverTraits<3,1> >Block;
    //old version of g2o where use pointer not unique_ptr
//     Block::LinearSolverType* linearSolver = new g2o::LinearSolverDense<Block::PoseMatrixType>();
//     Block* solver_ptr = new Block(linearSolver);
//     g2o::OptimizationAlgorithmLevenberg* solver=new g2o::OptimizationAlgorithmLevenberg(solver_ptr);
    
    std::unique_ptr<Block::LinearSolverType> linearSolver(new g2o::LinearSolverDense<Block::PoseMatrixType>());
    std::unique_ptr<Block> solver_ptr ( new Block ( std::move(linearSolver))); 
     g2o::OptimizationAlgorithmLevenberg* solver = new g2o::OptimizationAlgorithmLevenberg(std::move(solver_ptr));

    g2o::SparseOptimizer optimizer;//the graph model
    optimizer.setAlgorithm(solver); // set the solver
    optimizer.setVerbose(true);// the debug output
    
    //add vertex
    CurveFittingVertex* v = new CurveFittingVertex();
    v->setEstimate(Eigen::Vector3d(0,0,0));
    v->setId(0);
    optimizer.addVertex(v);

    //add edge to the graph
    for(int i=0;i<N;i++)
    {
        CurveFittingEdge* edge= new CurveFittingEdge(x_data[i]);
        edge->setId(i);
        edge->setVertex(0,v); // set the vertex to be connected
        edge->setMeasurement(y_data[i]);
        //Information matrix: the inverse of the covariance
        edge->setInformation(Eigen::Matrix<double,1,1>::Identity()*1/(w_sigma*w_sigma));
        optimizer.addEdge(edge);
    }
    cout<<"start optimization"<<endl;
    chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
    optimizer.initializeOptimization();
    optimizer.optimize(100);
    chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
    chrono::duration<double> time_used = chrono::duration_cast<chrono::duration<double> >( t2-t1 );
    cout<<"solve time cost = "<<time_used.count()<<" seconds. "<<endl;
    
    Eigen::Vector3d abc_estimate = v->estimate();
    cout<<"estimated model: "<<abc_estimate.transpose()<<endl;
    
    return 0;

}
