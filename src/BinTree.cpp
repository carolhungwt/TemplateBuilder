#include "BinTree.h"

#include "TAxis.h"
#include "TH3F.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <limits>
#include <map>

using namespace std;


struct pair_comp
{
   bool operator()( const pair<double, int>& lhs, const double& rhs ) const 
   { 
        return lhs.first < rhs;
   }
   bool operator()( const double& lhs, const pair<double, int>& rhs ) const 
   {
        return lhs < rhs.first;
   }
   bool operator()( const pair<double, int>& lhs, const pair<double, int>& rhs ) const 
   {
        return lhs.first < rhs.first;
   }
};


/*****************************************************************/
EntryList::EntryList(int ndim):
    m_ndim(ndim),
    m_maxWeight(0.),
    m_sumOfWeights(0.),
    m_sumOfWeightsError(0.)
/*****************************************************************/
{
    m_sortedValues.resize(ndim);
}


/*****************************************************************/
void EntryList::add(const std::vector<double>& values, double weight)
/*****************************************************************/
{
    int n = m_weights.size();
    vector<int> pos;
    for(unsigned int d=0;d<m_ndim;d++)
    {
        pair<double,int> p = make_pair(values[d], n);
        m_sortedValues[d].push_back(p);
        pos.push_back(n);
    }
    m_sortedPositions.push_back(pos);
    m_weights.push_back(weight);
}

/*****************************************************************/
unsigned int EntryList::size() const
/*****************************************************************/
{
    return m_weights.size();
}

/*****************************************************************/
unsigned int EntryList::effectiveSize() const
/*****************************************************************/
{
    double relError = sumOfWeightsError()/sumOfWeights();
    //double effNEntries = sqrt((double)size())/relError;
    double effNEntries = 1./(relError*relError); // number of entries that give the same relative error as the sum of weights
    return (unsigned int)effNEntries;
}


/*****************************************************************/
double EntryList::sumOfWeights() const
/*****************************************************************/
{
    return m_sumOfWeights;
}

/*****************************************************************/
double EntryList::sumOfWeightsError() const
/*****************************************************************/
{
    return m_sumOfWeightsError;
}

/*****************************************************************/
double EntryList::maxWeight() const
/*****************************************************************/
{
    return m_maxWeight;
}

/*****************************************************************/
double EntryList::value(unsigned int axis, int entry) const
/*****************************************************************/
{
    int pos = m_sortedPositions[entry][axis];
    return m_sortedValues[axis][pos].first;
}

/*****************************************************************/
double EntryList::weight(int entry) const
/*****************************************************************/
{
    return m_weights[entry];
}


/*****************************************************************/
void EntryList::sort()
/*****************************************************************/
{
    int nentries = m_weights.size();
    for(unsigned int d=0;d<m_ndim; d++)
    {
        // sort values in each dimension
        std::sort(m_sortedValues[d].begin(), m_sortedValues[d].end(), pair_comp());
        // then compute the map of sorted positions
        for(int e=0; e<nentries; e++)
        {
            int pos = m_sortedValues[d][e].second;
            m_sortedPositions[pos][d] = e;
        }
    }
    // compute sum of weights, sum of weight stat. uncertainty and maximum weight
    double sumw = 0.;
    double sumw2 = 0.;
    double maxw = 0.;
    for(unsigned int e=0; e<m_weights.size();e++)
    {
        sumw += m_weights[e];
        sumw2 += m_weights[e]*m_weights[e];
        if(m_weights[e]>maxw) maxw = m_weights[e];
    }
    m_sumOfWeights = sumw;
    m_maxWeight = maxw;
    m_sumOfWeightsError = sqrt(sumw2);
}

/*****************************************************************/
std::pair<EntryList, EntryList> EntryList::split(unsigned int axis, double cut) const
/*****************************************************************/
{
    EntryList leftList(m_ndim);
    EntryList rightList(m_ndim);
    vector< pair<double,int> >::const_iterator begin = m_sortedValues[axis].begin();
    vector< pair<double,int> >::const_iterator end = m_sortedValues[axis].end();
    vector< pair<double,int> >::const_iterator it;
    // loop over values and separate entries to the left and right wrt. the cut
    for(it=begin;it!=end;++it)
    {
        int index = it->second;
        double value = it->first;
        vector<double> values;
        for(unsigned int d=0;d<m_ndim;d++)
        {
            // use the position map to group together the different values of one entry
            values.push_back( m_sortedValues[d][m_sortedPositions[index][d]].first ); 
        }
        double weight = m_weights[index];
        if(value<cut)
        {
            leftList.add(values, weight);
        }
        else
        {
            rightList.add(values, weight);
        }
    }
    // The dimension of the cut is already sorted but not the others. So sort the two sub lists.
    leftList.sort();
    rightList.sort();
    return make_pair(leftList, rightList);

}

/*****************************************************************/
std::pair<int, int> EntryList::entriesIfSplit(unsigned int axis, double cut) const
/*****************************************************************/
{
    vector< pair<double,int> >::const_iterator begin = m_sortedValues[axis].begin();
    vector< pair<double,int> >::const_iterator end = m_sortedValues[axis].end();
    vector< pair<double,int> >::const_iterator splitpos = std::lower_bound(begin, end, cut, pair_comp());
    int leftEntries  = splitpos-begin;
    int rightEntries = end-splitpos;
    return make_pair(leftEntries, rightEntries);
}


/*****************************************************************/
std::vector<double> EntryList::percentiles(const std::vector<double>& qs, unsigned int axis) const
/*****************************************************************/
{
    vector<double> qscopy = qs;
    // make sure the quantiles are in increasing order
    std::sort(qscopy.begin(),qscopy.end());

    vector<double> ps(qscopy.size());
    for(unsigned int qi=0;qi<qscopy.size();qi++)
    {
        double q = qscopy[qi];
        // This doesn't take into account the entry weights, because it is much faster this way.
        // But it may be added in the future, with a percentiles cache in order to avoid their calculation each time.
        pair<double,int> valpos = *(m_sortedValues[axis].begin()+int(m_sortedValues[axis].size()*q/100.));
        ps[qi] = valpos.first;
    }
    return ps;
}

/*****************************************************************/
double EntryList::densityGradient(unsigned int axis, double q) const
/*****************************************************************/
{
    int ntot = m_weights.size(); 
    vector<double> qs;
    double qmulti = q;
    while(qmulti<100)
    {
        qs.push_back(qmulti);
        qmulti += q;
    }
    // Filling percentile array
    vector<double> pX = percentiles(qs,axis);
    pX.insert(pX.begin(), m_sortedValues[axis][0].first);
    pX.push_back(m_sortedValues[axis][ntot-1].first);

    double minDensity = numeric_limits<double>::max();
    double maxDensity = 0.;
    for(unsigned int i=0;i<pX.size()-1;i++)
    {
        double px1 = pX[i];
        double px2 = pX[i+1];
        // Number of entries between two percentiles divided by the distance between them
        double density = ( (float)ntot*(float)q/100. )/(px2-px1);
        if(density<minDensity) minDensity = density;
        if(density>maxDensity) maxDensity = density;
    }
    // Max density difference
    double gradient = fabs(maxDensity-minDensity);
    return gradient;
}


/*****************************************************************/
void EntryList::print()
/*****************************************************************/
{
    cerr<<"Printing entry list\n";
    cerr<<"  "<<m_ndim<<" dimensions, "<<size()<<" entries\n";
    int ntot = size();
    int step = ntot/10;
    for(unsigned int d=0;d<m_ndim;d++)
    {
        cerr<<"[";
        for(int e=0;e<ntot;e+=step)
        {
            cerr<<m_sortedValues[d][e].first<<"...";
        }
        cerr<<"]\n";
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

/*****************************************************************/
BinLeaf::BinLeaf():
    m_ndim(2),
    m_index(0),
    m_entryList(2)
/*****************************************************************/
{

    m_binBoundaries.push_back(make_pair(0.,1.));
    m_binBoundaries.push_back(make_pair(0.,1.));
}

/*****************************************************************/
BinLeaf::~BinLeaf()
/*****************************************************************/
{
    
}

/*****************************************************************/
BinLeaf::BinLeaf(const std::vector< std::pair<double,double> >& minmax):
    m_ndim(minmax.size()),
    m_index(0),
    m_entryList(minmax.size())
/*****************************************************************/
{
    unsigned int ndim = minmax.size();
    for(unsigned int axis=0;axis<ndim;axis++)
    {
        if(minmax[axis].first==minmax[axis].second)
        {
            stringstream error;
            error << "BinLeaf::BinLeaf(): Trying to build a bin with zero width ("<<minmax[axis].first<<","<<minmax[axis].second<<")";
            throw runtime_error(error.str());
        }
    }

    for(unsigned int axis=0;axis<ndim;axis++)
    {
        m_binBoundaries.push_back(make_pair(minmax[axis].first,minmax[axis].second));
    }
}

/*****************************************************************/
void BinLeaf::setBinBoundaries(const std::vector< std::pair<double,double> >& minmax)
/*****************************************************************/
{
    m_binBoundaries.clear();
    for(unsigned int axis=0;axis<m_ndim;axis++)
    {
        m_binBoundaries.push_back(make_pair(minmax[axis].first,minmax[axis].second));
    }
    m_ndim = m_binBoundaries.size();
}

/*****************************************************************/
const std::vector< std::pair<double,double> >& BinLeaf::getBinBoundaries()
/*****************************************************************/
{
    return m_binBoundaries;
}
/*****************************************************************/
double BinLeaf::getMin(int axis)
/*****************************************************************/
{
    return m_binBoundaries[axis].first;
}

/*****************************************************************/
double BinLeaf::getMax(int axis)
/*****************************************************************/
{
    return m_binBoundaries[axis].second;
}


/*****************************************************************/
double BinLeaf::getWidth(int axis)
/*****************************************************************/
{
    return (getMax(axis)-getMin(axis));
}


/*****************************************************************/
double BinLeaf::getCenter(int axis)
/*****************************************************************/
{
    return (getMax(axis)+getMin(axis))/2.;
}



/*****************************************************************/
bool BinLeaf::isNeighbor(BinLeaf* leaf) 
/*****************************************************************/
{
    bool neighbor = false;
    // check if borders are touching
    for(unsigned int axis=0;axis<m_ndim;axis++)
    {
        if( fabs((leaf->getMin(axis)-getMax(axis))/getMax(axis))<1.e-10 )
        {
            for(unsigned int axis2=0;axis2<m_ndim;axis2++)
            {
                if(axis2!=axis)
                {
                    if(leaf->getMax(axis2)>getMin(axis2) && leaf->getMin(axis2)<getMax(axis2) ) neighbor = true;
                }
            }
        }
        if( fabs((leaf->getMax(axis)-getMin(axis))/getMin(axis))<1.e-10 )
        {
            for(unsigned int axis2=0;axis2<m_ndim;axis2++)
            {
                if(axis2!=axis)
                {
                    if(leaf->getMax(axis2)>getMin(axis2) && leaf->getMin(axis2)<getMax(axis2) ) neighbor = true;
                }
            }
        }
    }
    
    return neighbor;
}


/*****************************************************************/
unsigned int BinLeaf::getNEntries()
/*****************************************************************/
{
    return m_entryList.size();
}

/*****************************************************************/
unsigned int BinLeaf::effectiveNEntries()
/*****************************************************************/
{
    return m_entryList.effectiveSize();
}

/*****************************************************************/
double BinLeaf::getSumOfWeights()
/*****************************************************************/
{
    return m_entryList.sumOfWeights();
}

/*****************************************************************/
const EntryList& BinLeaf::getEntries()
/*****************************************************************/
{
    return m_entryList;
}

///*****************************************************************/
//const std::vector< std::vector<double> >& BinLeaf::getEntries()
///*****************************************************************/
//{
//    return m_entries;
//}

///*****************************************************************/
//const std::vector< double >& BinLeaf::getWeights()
///*****************************************************************/
//{
//    return m_weights;
//}

/*****************************************************************/
std::vector<double> BinLeaf::percentiles(const std::vector<double>& q, unsigned int axis)
/*****************************************************************/
{
    return m_entryList.percentiles(q, axis);
}

/*****************************************************************/
double BinLeaf::densityGradient(unsigned int axis, double q)
/*****************************************************************/
{
    return m_entryList.densityGradient(axis,q);
}


/*****************************************************************/
bool BinLeaf::inBin(const std::vector<double>& xs)
/*****************************************************************/
{
    if(xs.size()!=m_ndim)
    {
        cout<<"[WARN]   BinLeaf::inBin(): number of dimensions doesn't match\n";
        return false;
    }
    bool isInBin = true;
    for(unsigned int axis=0;axis<m_ndim;axis++)
    {
        if(xs[axis]<getMin(axis) || xs[axis]>getMax(axis)) isInBin = false;
    }
    return isInBin;
}

/*****************************************************************/
bool BinLeaf::addEntry(const std::vector<double>& xsi, double wi)
/*****************************************************************/
{
    if(xsi.size()!=m_ndim)
    {
        cout<<"[WARN]   BinLeaf::addEntry(): number of dimensions doesn't match\n";
        return false;
    }
    // Check if it is contained in the bin
    if(inBin(xsi))
    {
        m_entryList.add(xsi,wi);
        return true;
    }
    return false;
}


/*****************************************************************/
void BinLeaf::setEntries(const EntryList& entries)
/*****************************************************************/
{
    m_entryList = entries;
}

/*****************************************************************/
void BinLeaf::sortEntries()
/*****************************************************************/
{
    m_entryList.sort();
}

/*****************************************************************/
std::vector<TLine*> BinLeaf::getBoundaryTLines()
/*****************************************************************/
{
    vector<TLine*> lines;
    if(m_ndim==2)
    {
        TLine* line1 = new TLine(getMin(0), getMin(1), getMin(0), getMax(1));
        TLine* line2 = new TLine(getMin(0), getMax(1), getMax(0), getMax(1));
        TLine* line3 = new TLine(getMax(0), getMax(1), getMax(0), getMin(1));
        TLine* line4 = new TLine(getMax(0), getMin(1), getMin(0), getMin(1));
        lines.push_back(line1);
        lines.push_back(line2);
        lines.push_back(line3);
        lines.push_back(line4);
    }
    return lines;
}


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////



/*****************************************************************/
BinTree::BinTree(const std::vector< std::pair<double,double> >& minmax, const std::vector< std::vector<double> >& entries, const std::vector< double >& weights)
/*****************************************************************/
{
    m_treeSons.push_back(NULL);
    m_treeSons.push_back(NULL);
    m_cutAxis = 0;
    m_cut = 0.;
    m_leaf = new BinLeaf(minmax);
    for(unsigned int e=0;e<entries.size();e++)
    {
        m_leaf->addEntry(entries[e], weights[e]);
    }
    m_ndim = minmax.size();
    for(unsigned int axis=0;axis<m_ndim;axis++)
    {
        m_vetoSplit.push_back(false);
    }
    m_minLeafEntries = 200;
    m_maxAxisAsymmetry = 2.;
    m_gridConstraint = NULL;
}

/*****************************************************************/
BinTree::~BinTree()
/*****************************************************************/
{
    //if(m_gridConstraint)
    //{
    //    m_gridConstraint->Delete();
    //    m_gridConstraint = NULL;
    //}
    if(m_leaf)
    {
        delete m_leaf;
    }
    else
    {
        delete m_treeSons[0];
        delete m_treeSons[1];
    }
    
}

void BinTree::setGridConstraint(TH1* gridConstraint) 
{
    //if(m_gridConstraint)
    //{
    //    m_gridConstraint->Delete();
    //    m_gridConstraint = NULL;
    //}
    //m_gridConstraint = (TH2F*)gridConstraint->Clone("gridConstraint");
    //m_gridConstraint->SetDirectory(0);
    m_gridConstraint = gridConstraint;
}

/*****************************************************************/
void BinTree::addEntry(const std::vector<double>& xsi, double wi)
/*****************************************************************/
{
    m_leaf->addEntry(xsi, wi);
}


/*****************************************************************/
std::vector< std::pair<double,double> > BinTree::getBinBoundaries()
/*****************************************************************/
{
    if(m_leaf)
    {
        return m_leaf->getBinBoundaries();
    }
    else
    {
        vector< pair<double, double> > boundaries;
        for(unsigned int axis=0;axis<m_ndim;axis++)
        {
            double min = getMin(axis);
            double max = getMax(axis);
            boundaries.push_back(make_pair(min,max));
        }
        return boundaries;
    }
}

/*****************************************************************/
double BinTree::getMin(int axis)
/*****************************************************************/
{
    if(m_leaf)
    {
        return m_leaf->getMin(axis);
    }
    else
    {
        double xmin1 = m_treeSons[0]->getMin(axis);
        double xmin2 = m_treeSons[1]->getMin(axis);
        double xmin = min(xmin1,xmin2);
        return xmin;
    }
}

/*****************************************************************/
double BinTree::getMax(int axis)
/*****************************************************************/
{
    if(m_leaf)
    {
        return m_leaf->getMax(axis);
    }
    else
    {
        double xmax1 = m_treeSons[0]->getMax(axis);
        double xmax2 = m_treeSons[1]->getMax(axis);
        double xmax = max(xmax1,xmax2);
        return xmax;
    }
}



/*****************************************************************/
unsigned int BinTree::getNEntries()
/*****************************************************************/
{
    if(m_leaf)
    {
        return m_leaf->getNEntries();
    }
    else
    {
        unsigned int nentries = m_treeSons[0]->getNEntries();
        nentries += m_treeSons[1]->getNEntries();
        return nentries;
    }
}


/*****************************************************************/
double BinTree::getSumOfWeights()
/*****************************************************************/
{
    if(m_leaf)
    {
        return m_leaf->getSumOfWeights();
    }
    else
    {
        double sumw = m_treeSons[0]->getSumOfWeights();
        sumw += m_treeSons[1]->getSumOfWeights();
        return sumw;
    }
}


///*****************************************************************/
//std::vector< std::vector<double> > BinTree::getEntries()
///*****************************************************************/
//{
//    if(m_leaf)
//    {
//        return m_leaf->getEntries();
//    }
//    else
//    {
//        vector< vector<double> > entries;
//        entries.insert(entries.end(),m_treeSons[0]->getEntries().begin(),m_treeSons[0]->getEntries().end());
//        entries.insert(entries.end(),m_treeSons[1]->getEntries().begin(),m_treeSons[1]->getEntries().end());
//        return entries;
//    }
//}

/*****************************************************************/
BinLeaf* BinTree::getLeaf(const std::vector<double>& xs)
/*****************************************************************/
{
    if(m_leaf)
    {
        if(m_leaf->inBin(xs))
        {
            return m_leaf;
        }
        else
        {
            return NULL;
        }
    }
    if(xs[m_cutAxis]<m_cut)
    {
        return m_treeSons[0]->getLeaf(xs);
    }
    else
    {
        return m_treeSons[1]->getLeaf(xs);
    }
}


/*****************************************************************/
std::vector<BinLeaf*> BinTree::getLeaves()
/*****************************************************************/
{
    vector<BinLeaf*> leaves;
    if(m_leaf)
    {
        leaves.push_back(m_leaf);
        return leaves;
    }
    else
    {
        vector<BinLeaf*> leaves1 = m_treeSons[0]->getLeaves();
        vector<BinLeaf*> leaves2 = m_treeSons[1]->getLeaves();
        leaves.insert(leaves.end(),leaves1.begin(), leaves1.end());
        leaves.insert(leaves.end(),leaves2.begin(), leaves2.end());
        return leaves;
    }
}


/*****************************************************************/
std::vector<BinTree*> BinTree::getTerminalNodes()
/*****************************************************************/
{
    vector<BinTree*> nodes;
    if(m_leaf)
    {
        nodes.push_back(this);
        return nodes;
    }
    else
    {
        vector<BinTree*> nodes1 = m_treeSons[0]->getTerminalNodes();
        vector<BinTree*> nodes2 = m_treeSons[1]->getTerminalNodes();
        nodes.insert(nodes.end(),nodes1.begin(), nodes1.end());
        nodes.insert(nodes.end(),nodes2.begin(), nodes2.end());
        return nodes;
    }
}

/*****************************************************************/
std::vector<BinLeaf*> BinTree::findNeighborLeaves(BinLeaf* leaf)
/*****************************************************************/
{
    vector<BinLeaf*> neighborLeaves;
    vector<BinLeaf*> allleaves = getLeaves();
    vector<BinLeaf*>::iterator it = allleaves.begin();
    vector<BinLeaf*>::iterator itE = allleaves.end();
    for(;it!=itE;++it)
    {
        bool isNeighbor = (*it)->isNeighbor(leaf);
        if(isNeighbor)
        {
            neighborLeaves.push_back(*it);
        }
    }
    return neighborLeaves;
}


/*****************************************************************/
unsigned int BinTree::getNLeaves()
/*****************************************************************/
{
    return getLeaves().size();
}

/*****************************************************************/
double BinTree::getMinBinWidth(unsigned int axis)
/*****************************************************************/
{
    if(m_leaf)
    {
        return m_leaf->getWidth(axis);
    }
    else
    {
        double width1 = m_treeSons[0]->getMinBinWidth(axis);
        double width2 = m_treeSons[1]->getMinBinWidth(axis);
        double width = min(width1,width2);
        return width;
    }
}

/*****************************************************************/
double BinTree::getMinEntries()
/*****************************************************************/
{
    if(m_leaf)
    {
        return m_leaf->getNEntries();
    }
    else
    {
        double nentries1 = m_treeSons[0]->getMinEntries();
        double nentries2 = m_treeSons[1]->getMinEntries();
        double nentries = min(nentries1,nentries2);
        return nentries;
    }
}

/*****************************************************************/
double BinTree::getMaxEntries()
/*****************************************************************/
{
    if(m_leaf)
    {
        return m_leaf->getNEntries();
    }
    else
    {
        double nentries1 = m_treeSons[0]->getMaxEntries();
        double nentries2 = m_treeSons[1]->getMaxEntries();
        double nentries = max(nentries1,nentries2);
        return nentries;
    }
}


/*****************************************************************/
unsigned int BinTree::maxLeafIndex()
/*****************************************************************/
{
    if(m_leaf)
    {
        return m_leaf->index();
    }
    else
    {
        return max(m_treeSons[0]->maxLeafIndex(), m_treeSons[1]->maxLeafIndex());
    }
}


/*****************************************************************/
pair<int,int> BinTree::entriesIfSplit(double cut, unsigned int axis)
/*****************************************************************/
{
    if(!m_leaf)
    {
        throw runtime_error("BinTree::entriesIfSplit(): This method can only be applied on terminal nodes");
    }
    // Cannot split the bin if the cut value is not contained within the bin boundaries
    if(cut<=getBinBoundaries()[axis].first || cut>=getBinBoundaries()[axis].second)
    {
        stringstream error;
        error<<"BinTree::entriesIfSplit(): Trying to split a node outside its bin boundaries. "<<cut<<"!=("<<getBinBoundaries()[axis].first<<","<<getBinBoundaries()[axis].second<<")";
        throw runtime_error(error.str());
    }
    return m_leaf->getEntries().entriesIfSplit(axis, cut);
}

/*****************************************************************/
void BinTree::splitLeaf(double cut, unsigned int maxLeafIndex, unsigned int axis)
/*****************************************************************/
{
        if(!m_leaf)
        {
            stringstream error;
            error << "BinTree.split(): This method can only be applied on terminal nodes";
            throw runtime_error(error.str());
        }
        // Cannot split the bin if the cut value is not contained within the bin boundaries
        if(cut<=getBinBoundaries()[axis].first || cut>=getBinBoundaries()[axis].second)
        {
            stringstream error;
            error << "BinTree.split(): Trying to split a node outside its bin boundaries. "<<cut<<"!=("<<getBinBoundaries()[axis].first<<","<<getBinBoundaries()[axis].second<<")";
            throw runtime_error(error.str());
        }
        // Create two neighbour bins
        m_cutAxis = axis;
        m_cut = cut;
        vector< pair<double,double> > boundaries1 = m_leaf->getBinBoundaries();
        vector< pair<double,double> > boundaries2 = m_leaf->getBinBoundaries();
        boundaries1[axis].second = cut;
        boundaries2[axis].first = cut;
        vector< vector<double> > emptyEntries;
        vector< double > emptyWeights;
        m_treeSons[0] = new BinTree(boundaries1, emptyEntries, emptyWeights);
        m_treeSons[1] = new BinTree(boundaries2, emptyEntries, emptyWeights);
        m_treeSons[0]->setMinLeafEntries(m_minLeafEntries);
        m_treeSons[1]->setMinLeafEntries(m_minLeafEntries);
        m_treeSons[0]->setMaxAxisAsymmetry(m_maxAxisAsymmetry);
        m_treeSons[1]->setMaxAxisAsymmetry(m_maxAxisAsymmetry);
        // Fill the two leaves that have just been created with entries of the parent node
        pair<EntryList,EntryList> entryLists = m_leaf->getEntries().split(axis, cut);
        m_treeSons[0]->leaf()->setEntries(entryLists.first);
        m_treeSons[1]->leaf()->setEntries(entryLists.second);
        // Assign new leaf indices
        m_treeSons[0]->leaf()->setIndex(maxLeafIndex+1);
        m_treeSons[1]->leaf()->setIndex(maxLeafIndex+2);
        // Set grid constraint
        m_treeSons[0]->setGridConstraint(m_gridConstraint);
        m_treeSons[1]->setGridConstraint(m_gridConstraint);
        // Finally destroy the old leaf. The node is not a terminal node anymore
        delete m_leaf;
        m_leaf = NULL;
}


/*****************************************************************/
void BinTree::findBestSplit(BinTree*& bestNode, unsigned int& axis, double& gradient)
/*****************************************************************/
{
    bestNode = NULL;
    //unsigned int bestAxis = 0;
    // If the node is terminal, only the axis needs to be chosen
    if(m_leaf)
    {
        // Don't split if the bin contains less than 2 times the minimum number of entries
        //if(getNEntries()<2.*m_minLeafEntries)
        if(m_leaf->effectiveNEntries()<2.*m_minLeafEntries)
        {
            bestNode = NULL;
            axis = 0;
            gradient = 0.;
            return;
        }
        // Best node is self since this is a terminal node
        bestNode = this;
        // Compute the density gradients along the axis
        // The best axis is the one with the largest gradient
        double maxgrad = 0.;
        int bestAxis = -1;
        for(unsigned int ax=0;ax<m_ndim;ax++)
        {
            double grad = m_leaf->densityGradient(ax);
            if(grad>maxgrad && !m_vetoSplit[ax]) // best gradient and no veto
            {
                maxgrad = grad;
                bestAxis = (int)ax;
            }
        }
        if(bestAxis==-1 || maxgrad==0)
        {
            bestNode = NULL;
            axis = 0;
            gradient = 0.;
            return;
        }
        axis = bestAxis;
        gradient = maxgrad;
        return;
    }
    // If the node is non-terminal, look inside the two sons
    else
    {
        BinTree* bestNode1 = NULL;
        BinTree* bestNode2 = NULL;
        unsigned int bestAxis1 = 0;
        unsigned int bestAxis2 = 0;
        double bestGrad1 = 0.;
        double bestGrad2 = 0.;
        m_treeSons[0]->findBestSplit(bestNode1, bestAxis1, bestGrad1);
        m_treeSons[1]->findBestSplit(bestNode2, bestAxis2, bestGrad2);
        // Look at the largest gradient
        if(bestGrad1>bestGrad2)
        {
            bestNode = bestNode1;
            axis = bestAxis1;
            gradient = bestGrad1;
            return;
        }
        else
        {
            bestNode = bestNode2;
            axis = bestAxis2;
            gradient = bestGrad2;
            return;
        }
    }
}


/*****************************************************************/
void BinTree::constrainSplit(int axis, double& cut, bool& veto)
/*****************************************************************/
{
    if(m_gridConstraint && !m_vetoSplit[axis])
    {
        TAxis* gridAxis = NULL;
        if(axis==0)
        {
            gridAxis = m_gridConstraint->GetXaxis();
        }
        else if(axis==1)
        {
            gridAxis = m_gridConstraint->GetYaxis();
        }
        else if(axis==2)
        {
            gridAxis = m_gridConstraint->GetZaxis();
        }
        else
        {
            stringstream error;
            error << "BinTree::constrainSplit(): Cannot use grid constrain for more than 3D";
            throw runtime_error(error.str());
        }
        // Find the closest grid constraint for the cut
        // And modify the cut according to this constraint
        int b   = gridAxis->FindBin(cut);
        double low = gridAxis->GetBinLowEdge(b);
        double up  = gridAxis->GetBinUpEdge(b);
        if(fabs(up-cut)<fabs(cut-low))
        {
            cut = up;
            // If the constrained cut is outside the bin boundaries, try the other grid constraint
            if(cut>=getBinBoundaries()[axis].second)
            {
                cut = low;
            }
        }
        else
        {
            cut = low;
            // If the constrained cut is outside the bin boundaries, try the other grid constraint
            if(cut<=getBinBoundaries()[axis].first)
            {
                cut = up;
            }
        }
        //  If the constrained cut is still outside the bin boundaries, veto this bin and axis
        if(cut<=getBinBoundaries()[axis].first || cut>=getBinBoundaries()[axis].second)
        {
            m_vetoSplit[axis] = true;
        }
    }
    veto = m_vetoSplit[axis];
}


/*****************************************************************/
void BinTree::minimizeLongBins(BinTree* tree, unsigned int axis, double& cut, bool& veto)
/*****************************************************************/
{
    if(!tree->vetoSplit(axis))
    {
        vector< pair<double,double> > binBoundaries = tree->getBinBoundaries();
        // this is supposed to be the root tree
        vector< pair<double,double> > fullBoundaries = getBinBoundaries();
        vector<double> fullLengths;
        vector<double> binRelLengths;
        for(unsigned int ax=0;ax<m_ndim;ax++)
        {
            fullLengths.push_back( fullBoundaries[ax].second-fullBoundaries[ax].first );
            binRelLengths.push_back( (binBoundaries[ax].second-binBoundaries[ax].first)/fullLengths[ax] );
        }
        //double binXRelLength = (binBoundaries[axis].second-binBoundaries[axis].first)/fullXLength;
        double cutRelDistance1 = (cut-binBoundaries[axis].first)/fullLengths[axis];
        double cutRelDistance2 = (binBoundaries[axis].second-cut)/fullLengths[axis];
        //unsigned int maxLengthAxis = 0;
        double maxRelLength = 0.;
        for(unsigned int ax=0;ax<m_ndim;ax++)
        {
            if(ax!=axis && binRelLengths[ax]>maxRelLength)
            {
                maxRelLength = binRelLengths[ax];
                //maxLengthAxis = ax;
            }
        }
        //cerr<<" axis="<<axis<<", cut="<<cut<<"\n";
        //cerr<<" cutreldistance = ("<<cutRelDistance1<<","<<cutRelDistance2<<") maxRelLength="<<maxRelLength<<"\n";
        if(cutRelDistance1<cutRelDistance2)
        {
            if(m_maxAxisAsymmetry*cutRelDistance1<maxRelLength)
            {
                cut = maxRelLength/m_maxAxisAsymmetry*fullLengths[axis]+binBoundaries[axis].first;
                cutRelDistance2 = (binBoundaries[axis].second-cut)/fullLengths[axis];
                //cerr<<" 1<max -> cut="<<cut<<"\n";
                if(cut>=binBoundaries[axis].second || m_maxAxisAsymmetry*cutRelDistance2<maxRelLength)
                {
                    tree->setVetoSplit(axis, true);
                    //cerr<<" veto\n";
                }
            }
        }
        else// cutRelDistance2<cutRelDistance1:
        {
            if(m_maxAxisAsymmetry*cutRelDistance2<maxRelLength)
            {
                cut = binBoundaries[axis].second-maxRelLength/m_maxAxisAsymmetry*fullLengths[axis];
                cutRelDistance1 = (cut-binBoundaries[axis].first)/fullLengths[axis];
                //cerr<<" 2<max -> cut="<<cut<<"\n";
                if(cut<=binBoundaries[axis].first || m_maxAxisAsymmetry*cutRelDistance1<maxRelLength)
                {
                    tree->setVetoSplit(axis,true);
                    //cerr<<" veto\n";
                }
            }
        }
    }
    veto = tree->vetoSplit(axis);
}


/*****************************************************************/
void BinTree::build()
/*****************************************************************/
{

    m_leaf->sortEntries();
    // If the tree already contains too small number of entries, it does nothing
    //if(getNEntries()<2.*m_minLeafEntries)
    //cerr<<"Effective number of entries = "<<m_leaf->effectiveNEntries()<<"\n";
    if(m_leaf->effectiveNEntries()<2.*m_minLeafEntries)
    {
        cout<<"[WARN] Total effective number of entries = "<<m_leaf->effectiveNEntries()<<" < 2 x "<<m_minLeafEntries<<". The procedure stops with one single bin\n";
        cout<<"[WARN]   You'll have to reduce the minimum number of entries per bin if you want to have more than one bin.\n";
        return;
    }
    // Start with first splitting
    BinTree* tree = NULL;
    unsigned int axis = 0;
    double grad = 0.;
    vector<double> perc50;
    perc50.push_back(50.);
    findBestSplit(tree, axis, grad);

    double cut = tree->leaf()->percentiles(perc50,axis)[0];
    // Modify cut according to grid constraints
    bool veto = false;
    tree->constrainSplit(axis, cut, veto);
    if(!veto)
    {
        tree->splitLeaf(cut, maxLeafIndex(), axis);
    }
    int nsplits = 1;
    //int totalEntries = getNEntries();
    //int previousMaxEntries = totalEntries;
    // Split until it is not possible to split (too small number of entries, or vetoed bins)
    while(tree)
    {
        findBestSplit(tree, axis, grad);
        // This is the end
        if(!tree)
        {
            break;
        }
        cut = tree->leaf()->percentiles(perc50,axis)[0];
        veto = false;
        //cerr<<"Bin ("<<tree->getMin(0)<<","<<tree->getMax(0)<<")("<<tree->getMin(1)<<","<<tree->getMax(1)<<")("<<tree->getMin(2)<<","<<tree->getMax(2)<<")\n";
        minimizeLongBins(tree, axis, cut,veto);
        //cerr<<" axis="<<axis<<", cut="<<cut<<"\n";
        if(!veto)
        {
            // Modify cut according to grid constraints
            tree->constrainSplit(axis, cut, veto);
            if(!veto)
            {
                tree->splitLeaf(cut, maxLeafIndex(), axis);
                nsplits += 1;
                cout<<"[INFO]   Number of bins = "<<getNLeaves()<<"\r"<<flush;
            }
        }
        //int maxEntries = getMaxEntries();

        //if( maxEntries!=previousMaxEntries)
        //{
        //    double n = (double)totalEntries/(double)m_minLeafEntries;
        //    double ratio  =  min( 1.-log((double)maxEntries/(double)m_minLeafEntries)/log(n),1.);
        //    int   c      =  ratio * 50;
        //    cout << "[INFO]   "<< setw(3) << (int)(ratio*100) << "% [";
        //    for (int x=0; x<c; x++) cout << "=";
        //    for (int x=c; x<50; x++) cout << " ";
        //    cout << "]\r" << flush;
        //    previousMaxEntries = maxEntries;
        //}

    }


    // Look if some bins are more than 50% empty. If it is the case, the empty part is separated (including one event)
    // from the part of the bin that contains all the entries
    // TODO: Maybe this is not necessary
    /*bool finish = false;
    vector<double> perc0;
    vector<double> perc100;
    perc0.push_back(0.);
    perc100.push_back(100.);
    while(!finish)
    {
        vector<BinTree*> terminalNodes = getTerminalNodes();
        for(unsigned int i=0;i<terminalNodes.size();i++)
        {
            BinTree* node = terminalNodes[i];
            BinLeaf* leaf = node->leaf();
            if(leaf->getNEntries()<=1)
            {
                continue;
            }
            vector< pair<double,double> > emptyFractions;
            vector< pair<double,double> > firstLastPoints;
            double maxEmptyFraction = 0.;
            int maxEmptyFractionAxis = 0;
            int maxEmptyFractionSide = 0;
            for(unsigned int axis=0;axis<m_ndim;axis++)
            {
                double firstPoint = leaf->percentiles(perc0,axis)[0];
                double lastPoint  = leaf->percentiles(perc100,axis)[0];
                firstLastPoints.push_back( make_pair(firstPoint, lastPoint) );
                double emptyFractionDown = (firstPoint-leaf->getMin(axis))/(leaf->getMax(axis)-leaf->getMin(axis));
                double emptyFractionUp   = (leaf->getMax(axis)-lastPoint)/(leaf->getMax(axis)-leaf->getMin(axis));
                emptyFractions.push_back( make_pair(emptyFractionDown, emptyFractionUp) );
                if(emptyFractionDown>maxEmptyFraction)
                {
                    maxEmptyFraction = emptyFractionDown;
                    maxEmptyFractionAxis = axis;
                    maxEmptyFractionSide = 0;
                }
                if(emptyFractionUp>maxEmptyFraction)
                {
                    maxEmptyFraction = emptyFractionUp;
                    maxEmptyFractionAxis = axis;
                    maxEmptyFractionSide = 1;
                }
            }
            // If the maximum empty fraction is more than 50%, then the bin is split
            if(maxEmptyFraction>0.5)
            {
                double cut = 0.;
                if(maxEmptyFractionSide==0) cut = firstLastPoints[maxEmptyFractionAxis].first;
                else if(maxEmptyFractionSide==1) cut = firstLastPoints[maxEmptyFractionAxis].second;

                //veto,cut = self.minimizeLongBins(node, cut, axis)
                bool veto = false;
                node->constrainSplit(maxEmptyFractionAxis,cut,veto);
                if(!veto)
                {
                    node->splitLeaf(cut, maxLeafIndex(), maxEmptyFractionAxis);
                }
                nsplits += 1;
            }
            else
            {
                finish = true;
            }
        }
    }*/

    // Split leaves close to the boundaries
    vector< std::pair<double,double> > boundaries = getBinBoundaries();
    vector<BinTree*> terminalNodes = getTerminalNodes();
    for(unsigned int i=0;i<terminalNodes.size();i++)
    {
        BinTree* node = terminalNodes[i];
        BinLeaf* leaf = node->leaf();
        int nSplitAxis = 0;
        vector<bool> splits;
        for(unsigned int axis=0;axis<m_ndim;axis++)
        {
            splits.push_back(false);
            if(leaf->getMin(axis)==boundaries[axis].first || leaf->getMax(axis)==boundaries[axis].second)
            {
                splits[axis] = true;
                nSplitAxis ++;
                //cout<<"Will split bin ["<<leaf->getMin(axis)<<","<<leaf->getMax(axis)<<"] along axis "<<axis<<"\n";
            }
        }
        vector<BinTree*> nodes;
        vector< pair<BinTree*,int> > nodesToSplitFurther;
        nodes.push_back(node);
        for(unsigned int axis=0;axis<m_ndim;axis++)
        {
            if(splits[axis])
            {
                vector<BinTree*> newNodes;
                for(unsigned int n=0;n<nodes.size();n++)
                {
                    BinTree* node = nodes[n];
                    double middle = (node->getMax(axis)+node->getMin(axis))/2.;
                    //cout<<"  Splitting border bin at "<<middle<<" on axis "<<axis<<"\n";
                    //cout<<"    Nentries before = "<<node->getNEntries()<<"\n";
                    pair<int,int> entriesAfterCut = node->entriesIfSplit(middle,axis);
                    //cout<<"    Nentries after = "<<entriesAfterCut.first<<"+"<<entriesAfterCut.second<<"\n";
                    //if(node->getNEntries()>0)
                    //{
                    //    cout<<"    Ratio = "<<(double)min(entriesAfterCut.first,entriesAfterCut.second)/(double)max(entriesAfterCut.first,entriesAfterCut.second)<<"\n";
                    //}
                    if(node->getNEntries()>0 && (double)min(entriesAfterCut.first,entriesAfterCut.second)/(double)max(entriesAfterCut.first,entriesAfterCut.second)<0.7)
                    {
                        node->splitLeaf(middle, maxLeafIndex(), axis);
                        newNodes.push_back(node->getSons()[0]);
                        newNodes.push_back(node->getSons()[1]);
                        //cout<<"    Splitting\n";
                        //cout<<"    Nentries after = "<<node->getSons()[0]->getNEntries()<<"+"<<node->getSons()[1]->getNEntries()<<"\n";
                        if(nSplitAxis==1)
                        //if(nSplitAxis==1 && min(node->getSons()[0]->getNEntries(),node->getSons()[1]->getNEntries())>0 &&
                        //        (double)min(node->getSons()[0]->getNEntries(),node->getSons()[1]->getNEntries())/(double)max(node->getSons()[0]->getNEntries(),node->getSons()[1]->getNEntries())<0.7)
                        {
                        //    cout<<"   Ratio = "<<(double)min(node->getSons()[0]->getNEntries(),node->getSons()[1]->getNEntries())/(double)max(node->getSons()[0]->getNEntries(),node->getSons()[1]->getNEntries())<<"\n";
                            nodesToSplitFurther.push_back(make_pair(node->getSons()[0],axis));
                            nodesToSplitFurther.push_back(make_pair(node->getSons()[1],axis));
                        }
                        nsplits += 1;
                    }
                }
                nodes.clear();
                for(unsigned int n=0;n<newNodes.size();n++)
                {
                    nodes.push_back(newNodes[n]);
                }
            }
        }
        // split bins further
        int ns = 0;
        while(nodesToSplitFurther.size()>0 && ns<2)
        {
            //cout<<"ns = "<<ns<<"\n";
            vector< pair<BinTree*,int> > newNodesToSplitFurther;
            for(unsigned int j=0;j<nodesToSplitFurther.size();j++)
            {
                BinTree* node2 = nodesToSplitFurther[j].first;
                int axis = nodesToSplitFurther[j].second;
                if(node2->getMin(axis)==boundaries[axis].first || node2->getMax(axis)==boundaries[axis].second)
                {
                    //cout<<"    Split bin ["<<node2->getMin(axis)<<","<<node2->getMax(axis)<<"] along axis "<<axis<<"\n";
                    double middle = (node2->getMax(axis)+node2->getMin(axis))/2.;
                    pair<int,int> entriesAfterCut = node2->entriesIfSplit(middle,axis);
                    //cout<<"    Nentries after = "<<entriesAfterCut.first<<"+"<<entriesAfterCut.second<<"\n";
                    //if(node2->getNEntries()>0)
                    //{
                    //    cout<<"    Ratio = "<<(double)min(entriesAfterCut.first,entriesAfterCut.second)/(double)max(entriesAfterCut.first,entriesAfterCut.second)<<"\n";
                    //}
                    if(node2->getNEntries()>0 && (double)min(entriesAfterCut.first,entriesAfterCut.second)/(double)max(entriesAfterCut.first,entriesAfterCut.second)<0.5)
                    {
                        node2->splitLeaf(middle, maxLeafIndex(), axis);
                        //cout<<"    Splitting\n";
                        nsplits += 1;
                        //if(nSplitAxis==1 && min(node2->getSons()[0]->getNEntries(),node2->getSons()[1]->getNEntries())>0 &&
                        //        (double)min(node2->getSons()[0]->getNEntries(),node2->getSons()[1]->getNEntries())/(double)max(node2->getSons()[0]->getNEntries(),node2->getSons()[1]->getNEntries())<0.7)
                        //{
                         //   cout<<"   Ratio = "<<(double)min(node2->getSons()[0]->getNEntries(),node2->getSons()[1]->getNEntries())/(double)max(node2->getSons()[0]->getNEntries(),node2->getSons()[1]->getNEntries())<<"\n";
                            newNodesToSplitFurther.push_back(make_pair(node2->getSons()[0],axis));
                            newNodesToSplitFurther.push_back(make_pair(node2->getSons()[1],axis));
                        //}
                    }
                }
            }
            nodesToSplitFurther.clear();
            for(unsigned int n=0;n<newNodesToSplitFurther.size();n++)
            {
                nodesToSplitFurther.push_back(newNodesToSplitFurther[n]);
            }
            ns ++;
        }
    }
}

/*****************************************************************/
std::vector<TLine*> BinTree::getBoundaryTLines()
/*****************************************************************/
{
    if(m_leaf)
    {
        return m_leaf->getBoundaryTLines();
    }
    else
    {
        vector<TLine*> lines;
        vector<TLine*> lines1 = m_treeSons[0]->getBoundaryTLines();
        vector<TLine*> lines2 = m_treeSons[1]->getBoundaryTLines();
        
        lines.insert(lines.end(),lines1.begin(), lines1.end());
        lines.insert(lines.end(),lines2.begin(), lines2.end());
        return lines;
    }
}


/*****************************************************************/
TH1* BinTree::fillHistogram()
/*****************************************************************/
{
        if(!m_gridConstraint)
        {
            stringstream error;
            error << "BinTree::fillHistogram(): Trying to fill histogram, but the binning is unknown. Define first the gridConstraint";
            throw runtime_error(error.str());
        }
        if(m_ndim>3)
        {
            stringstream error;
            error << "BinTree::fillHistogram(): Cannot fill histograms with more than 3 dimensions";
            throw runtime_error(error.str());
        }
        TH1* histo = (TH1*)m_gridConstraint->Clone("histoFromTree");
        int nbinsx = histo->GetNbinsX();
        int nbinsy = histo->GetNbinsY();
        int nbinsz = histo->GetNbinsZ();
        map<BinLeaf*, vector< vector<int> > > binsInLeaf;
        // First find the list of TH2 bins for each BinLeaf bin
        for(int bx=1;bx<nbinsx+1;bx++) 
        {
            for(int by=1;by<nbinsy+1;by++)
            {
                if(m_ndim==3)
                {
                    TH3F* h3f = dynamic_cast<TH3F*>(histo);
                    for(int bz=1;bz<nbinsz+1;bz++)
                    {
                        h3f->SetBinContent(bx,by,bz,0);
                        h3f->SetBinError(bx,by,bz,0);
                        double x = h3f->GetXaxis()->GetBinCenter(bx);
                        double y = h3f->GetYaxis()->GetBinCenter(by);
                        double z = h3f->GetZaxis()->GetBinCenter(bz);
                        vector<double> point;
                        point.push_back(x);
                        point.push_back(y);
                        point.push_back(z);
                        BinLeaf* leaf = getLeaf(point);
                        if(binsInLeaf.find(leaf)==binsInLeaf.end())
                        {
                            vector< vector<int> > empty;
                            binsInLeaf[leaf] = empty;
                        }
                        vector<int> bin;
                        bin.push_back(bx);
                        bin.push_back(by);
                        bin.push_back(bz);
                        binsInLeaf[leaf].push_back(bin);
                    }
                }
                else if(m_ndim==2)
                {
                    TH2F* h2f = dynamic_cast<TH2F*>(histo);
                    h2f->SetBinContent(bx,by,0);
                    h2f->SetBinError(bx,by,0);
                    double x = h2f->GetXaxis()->GetBinCenter(bx);
                    double y = h2f->GetYaxis()->GetBinCenter(by);
                    vector<double> point;
                    point.push_back(x);
                    point.push_back(y);
                    BinLeaf* leaf = getLeaf(point);
                    if(binsInLeaf.find(leaf)==binsInLeaf.end())
                    {
                        vector< vector<int> > empty;
                        binsInLeaf[leaf] = empty;
                    }
                    vector<int> bin;
                    bin.push_back(bx);
                    bin.push_back(by);
                    binsInLeaf[leaf].push_back(bin);
                }
            }
        }
        // Then all the TH2 bins are filled according to the entries in the BinLeaf bins
        map<BinLeaf*, vector< vector<int> > >::iterator it = binsInLeaf.begin();
        map<BinLeaf*, vector< vector<int> > >::iterator itE = binsInLeaf.end();
        for(;it!=itE;++it)
        {
            BinLeaf* leaf = it->first;
            vector< vector<int> > bins = it->second;
            const EntryList& entries = leaf->getEntries();
            int nbins = bins.size();
            for(int b=0;b<nbins;b++)
            {
                if(m_ndim==2)
                {
                    TH2F* h2f = dynamic_cast<TH2F*>(histo);
                    int bx = bins[b][0];
                    int by = bins[b][1];
                    double x = h2f->GetXaxis()->GetBinCenter(bx);
                    double y = h2f->GetYaxis()->GetBinCenter(by);
                    for(unsigned int e=0;e<entries.size();e++)
                    {
                        double value = entries.weight(e)/(double)nbins;
                        h2f->Fill(x,y,value);
                    }
                }
                else if(m_ndim==3)
                {
                    TH3F* h3f = dynamic_cast<TH3F*>(histo);
                    int bx = bins[b][0];
                    int by = bins[b][1];
                    int bz = bins[b][2];
                    double x = h3f->GetXaxis()->GetBinCenter(bx);
                    double y = h3f->GetYaxis()->GetBinCenter(by);
                    double z = h3f->GetZaxis()->GetBinCenter(bz);
                    for(unsigned int e=0;e<entries.size();e++)
                    {
                        double value = entries.weight(e)/(double)nbins;
                        h3f->Fill(x,y,z,value);
                    }
                }
            }
        }
        return histo;
}


/*****************************************************************/
vector<TH1*> BinTree::fillWidthsHighStat(const TH1* widthTemplate)
/*****************************************************************/
{
    if(!widthTemplate && !m_gridConstraint)
    {
        stringstream error;
        error << "BinTree::fillWidths(): Trying to fill width, but the binning is unknown. Please give a template histogram or define a gridConstraint";
        throw runtime_error(error.str());
    }
    if(m_ndim>3)
    {
        stringstream error;
        error << "BinTree::fillWidths(): Cannot fill histograms with more than 3 dimensions";
        throw runtime_error(error.str());
    }
    vector<TH1*> widths;
    if(m_ndim==2)
    {
        const TH1* gridRef = (widthTemplate ? widthTemplate : m_gridConstraint);
        TH1* hWidthX = (TH1*)gridRef->Clone("widthXFromTree");
        TH1* hWidthY = (TH1*)gridRef->Clone("widthYFromTree");
        int nbinsx = hWidthX->GetNbinsX();
        int nbinsy = hWidthX->GetNbinsY();
        int counter = 0;
        int total = nbinsx*nbinsy;
        for(int bx=1;bx<nbinsx+1;bx++) 
        {
            for(int by=1;by<nbinsy+1;by++)
            {
                counter++;
                if(counter % (total/100) == 0)
                {
                    double ratio  =  (double)counter/(double)total;
                    int   c      =  ratio * 50;
                    cout << "[INFO]   "<< setw(3) << (int)(ratio*100) << "% [";
                    for (int x=0; x<c; x++) cout << "=";
                    for (int x=c; x<50; x++) cout << " ";
                    cout << "]\r" << flush;
                }
                double x = hWidthX->GetXaxis()->GetBinCenter(bx);
                double y = hWidthX->GetYaxis()->GetBinCenter(by);
                vector<double> point;
                point.push_back(x);
                point.push_back(y);
                //cout<<"Computing width for ("<<x<<","<<y<<")\n";
                BinLeaf* leaf = getLeaf(point);
                vector<BinLeaf*> neighborLeaves = findNeighborLeaves(leaf);
                neighborLeaves.push_back(leaf);
                vector<BinLeaf*>::iterator itLeaf = neighborLeaves.begin();
                vector<BinLeaf*>::iterator itELeaf = neighborLeaves.end();
                double sumw = 0.;
                double sumwx = 0.;
                double sumwy = 0.;
                //cout<<" "<<neighborLeaves.size()<<" neighbor leaves\n";
                for(;itLeaf!=itELeaf;++itLeaf)
                {
                    double xi = (*itLeaf)->getCenter(0);
                    double yi = (*itLeaf)->getCenter(1);
                    double dx = fabs(xi-x);
                    double dy = fabs(yi-y);
                    double wxi = (*itLeaf)->getWidth(0);
                    double wyi = (*itLeaf)->getWidth(1);
                    if(dx<0.05*wxi) dx = 0.05*wxi;
                    if(dy<0.05*wyi) dy = 0.05*wyi;
                    double dr2 = dx*dx+dy*dy;
                    double dr = sqrt(dr2);
                    sumw += 1./dr;
                    sumwx += wxi/dr;
                    sumwy += wyi/dr;
                    //cout<<"  Neighbor leaf dx="<<dx<<",dy="<<dy<<",w="<<1./dr2<<"\n";
                }
                double widthx = sumwx/sumw;
                double widthy = sumwy/sumw;
                //cout<<" Width x="<<widthx<<",y="<<widthy<<"\n";
                hWidthX->SetBinContent(bx,by,widthx);
                hWidthY->SetBinContent(bx,by,widthy);
                hWidthX->SetBinError(bx,by,0.);
                hWidthY->SetBinError(bx,by,0.);
            }
        }
        widths.push_back(hWidthX);
        widths.push_back(hWidthY);
    }
    else if(m_ndim==3)
    {
        const TH1* gridRef = (widthTemplate ? widthTemplate : m_gridConstraint);
        TH1* hWidthX = (TH1*)gridRef->Clone("widthXFromTree");
        TH1* hWidthY = (TH1*)gridRef->Clone("widthYFromTree");
        TH1* hWidthZ = (TH1*)gridRef->Clone("widthZFromTree");
        int nbinsx = hWidthX->GetNbinsX();
        int nbinsy = hWidthX->GetNbinsY();
        int nbinsz = hWidthX->GetNbinsZ();
        int counter = 0;
        int total = nbinsx*nbinsy*nbinsz;
        for(int bx=1;bx<nbinsx+1;bx++) 
        {
            for(int by=1;by<nbinsy+1;by++)
            {
                for(int bz=1;bz<nbinsz+1;bz++)
                {
                    counter++;
                    if(counter % (total/100) == 0)
                    {
                        double ratio  =  (double)counter/(double)total;
                        int   c      =  ratio * 50;
                        cout << "[INFO]   "<< setw(3) << (int)(ratio*100) << "% [";
                        for (int x=0; x<c; x++) cout << "=";
                        for (int x=c; x<50; x++) cout << " ";
                        cout << "]\r" << flush;
                    }
                    double x = hWidthX->GetXaxis()->GetBinCenter(bx);
                    double y = hWidthX->GetYaxis()->GetBinCenter(by);
                    double z = hWidthX->GetZaxis()->GetBinCenter(bz);
                    vector<double> point;
                    point.push_back(x);
                    point.push_back(y);
                    point.push_back(z);
                    //cout<<"Computing width for ("<<x<<","<<y<<")\n";
                    BinLeaf* leaf = getLeaf(point);
                    vector<BinLeaf*> neighborLeaves = findNeighborLeaves(leaf);
                    neighborLeaves.push_back(leaf);
                    vector<BinLeaf*>::iterator itLeaf = neighborLeaves.begin();
                    vector<BinLeaf*>::iterator itELeaf = neighborLeaves.end();
                    double sumw = 0.;
                    double sumwx = 0.;
                    double sumwy = 0.;
                    double sumwz = 0.;
                    //cout<<" "<<neighborLeaves.size()<<" neighbor leaves\n";
                    for(;itLeaf!=itELeaf;++itLeaf)
                    {
                        double xi = (*itLeaf)->getCenter(0);
                        double yi = (*itLeaf)->getCenter(1);
                        double zi = (*itLeaf)->getCenter(2);
                        double dx = fabs(xi-x);
                        double dy = fabs(yi-y);
                        double dz = fabs(zi-z);
                        double wxi = (*itLeaf)->getWidth(0);
                        double wyi = (*itLeaf)->getWidth(1);
                        double wzi = (*itLeaf)->getWidth(2);
                        if(dx<0.05*wxi) dx = 0.05*wxi;
                        if(dy<0.05*wyi) dy = 0.05*wyi;
                        if(dz<0.05*wzi) dz = 0.05*wzi;
                        double dr2 = dx*dx+dy*dy+dz*dz;
                        double dr = sqrt(dr2);
                        sumw += 1./dr;
                        sumwx += wxi/dr;
                        sumwy += wyi/dr;
                        sumwz += wzi/dr;
                        //cout<<"  Neighbor leaf dx="<<dx<<",dy="<<dy<<",w="<<1./dr2<<"\n";
                    }
                    double widthx = sumwx/sumw;
                    double widthy = sumwy/sumw;
                    double widthz = sumwz/sumw;
                    hWidthX->SetBinContent(bx,by,bz,widthx);
                    hWidthY->SetBinContent(bx,by,bz,widthy);
                    hWidthZ->SetBinContent(bx,by,bz,widthz);
                    hWidthX->SetBinError(bx,by,bz,0.);
                    hWidthY->SetBinError(bx,by,bz,0.);
                    hWidthZ->SetBinError(bx,by,bz,0.);
                }
            }
        }
        widths.push_back(hWidthX);
        widths.push_back(hWidthY);
        widths.push_back(hWidthZ);
    }
    cout << "[INFO]   "<< setw(3) << 100 << "% [";
    for (int x=0; x<50; x++) cout << "=";
    cout << "]" << endl;
    return widths;
}

/*****************************************************************/
vector<TH1*> BinTree::fillWidthsLowStat(const TH1* widthTemplate)
/*****************************************************************/
{
    if(!widthTemplate && !m_gridConstraint)
    {
        stringstream error;
        error << "BinTree::fillWidths(): Trying to fill width, but the binning is unknown. Please give a template histogram or define a gridConstraint";
        throw runtime_error(error.str());
    }
    if(m_ndim>3)
    {
        stringstream error;
        error << "BinTree::fillWidths(): Cannot fill histograms with more than 3 dimensions";
        throw runtime_error(error.str());
    }
    vector<TH1*> widths;
    if(m_ndim==2)
    {
        const TH1* gridRef = (widthTemplate ? widthTemplate : m_gridConstraint);
        TH1* hWidthX = (TH1*)gridRef->Clone("widthXFromTree");
        TH1* hWidthY = (TH1*)gridRef->Clone("widthYFromTree");
        int nbinsx = hWidthX->GetNbinsX();
        int nbinsy = hWidthX->GetNbinsY();
        int counter = 0;
        int total = nbinsx*nbinsy;
        vector<BinLeaf*> neighborLeaves = getLeaves();
        for(int bx=1;bx<nbinsx+1;bx++) 
        {
            for(int by=1;by<nbinsy+1;by++)
            {
                counter++;
                if(counter % (total/100) == 0)
                {
                    double ratio  =  (double)counter/(double)total;
                    int   c      =  ratio * 50;
                    cout << "[INFO]   "<< setw(3) << (int)(ratio*100) << "% [";
                    for (int x=0; x<c; x++) cout << "=";
                    for (int x=c; x<50; x++) cout << " ";
                    cout << "]\r" << flush;
                }
                double x = hWidthX->GetXaxis()->GetBinCenter(bx);
                double y = hWidthX->GetYaxis()->GetBinCenter(by);
                double xBinWidth = hWidthX->GetXaxis()->GetBinWidth(bx);
                double yBinWidth = hWidthX->GetYaxis()->GetBinWidth(by);
                vector<double> point;
                point.push_back(x);
                point.push_back(y);
                //cout<<"Computing width for ("<<x<<","<<y<<")\n";
                BinLeaf* leaf = getLeaf(point);
                if(leaf->getWidth(0)<=xBinWidth && leaf->getWidth(1)<=yBinWidth) 
                {
                    hWidthX->SetBinContent(bx,by,leaf->getWidth(0));
                    hWidthY->SetBinContent(bx,by,leaf->getWidth(1));
                    hWidthX->SetBinError(bx,by,0.);
                    hWidthY->SetBinError(bx,by,0.);
                    continue;
                }
                vector<BinLeaf*>::iterator itLeaf = neighborLeaves.begin();
                vector<BinLeaf*>::iterator itELeaf = neighborLeaves.end();
                double sumw = 0.;
                double sumwx = 0.;
                double sumwy = 0.;
                //cout<<" "<<neighborLeaves.size()<<" neighbor leaves\n";
                double xRegionSize = getMax(0)-getMin(0);
                double yRegionSize = getMax(1)-getMin(1);
                for(;itLeaf!=itELeaf;++itLeaf)
                {
                    double xi = (*itLeaf)->getCenter(0);
                    double yi = (*itLeaf)->getCenter(1);
                    double dx = fabs(xi-x)/xRegionSize;
                    double dy = fabs(yi-y)/yRegionSize;
                    double wxi = (*itLeaf)->getWidth(0);
                    double wyi = (*itLeaf)->getWidth(1);
                    if(dx<0.001*wxi) dx = 0.001*wxi;
                    if(dy<0.001*wyi) dy = 0.001*wyi;
                    double dr2 = dx*dx+dy*dy;
                    //double dr = sqrt(dr2);
                    //double dr = pow(dr2,1./10.);
                    double dr = dr2;
                    sumw += 1./dr;
                    sumwx += wxi/dr;
                    sumwy += wyi/dr;
                    //cout<<"  Neighbor leaf dx="<<dx<<",dy="<<dy<<",w="<<1./dr<<"\n";
                }
                double widthx = sumwx/sumw;
                double widthy = sumwy/sumw;
                //cout<<" Width x="<<widthx<<",y="<<widthy<<"\n";
                hWidthX->SetBinContent(bx,by,widthx);
                hWidthY->SetBinContent(bx,by,widthy);
                hWidthX->SetBinError(bx,by,0.);
                hWidthY->SetBinError(bx,by,0.);
            }
        }
        widths.push_back(hWidthX);
        widths.push_back(hWidthY);
    }
    else if(m_ndim==3)
    {
        const TH1* gridRef = (widthTemplate ? widthTemplate : m_gridConstraint);
        TH1* hWidthX = (TH1*)gridRef->Clone("widthXFromTree");
        TH1* hWidthY = (TH1*)gridRef->Clone("widthYFromTree");
        TH1* hWidthZ = (TH1*)gridRef->Clone("widthZFromTree");
        int nbinsx = hWidthX->GetNbinsX();
        int nbinsy = hWidthX->GetNbinsY();
        int nbinsz = hWidthX->GetNbinsZ();
        int counter = 0;
        int total = nbinsx*nbinsy*nbinsz;
        vector<BinLeaf*> neighborLeaves = getLeaves();
        for(int bx=1;bx<nbinsx+1;bx++) 
        {
            for(int by=1;by<nbinsy+1;by++)
            {
                for(int bz=1;bz<nbinsz+1;bz++)
                {
                    counter++;
                    if(counter % (total/100) == 0)
                    {
                        double ratio  =  (double)counter/(double)total;
                        int   c      =  ratio * 50;
                        cout << "[INFO]   "<< setw(3) << (int)(ratio*100) << "% [";
                        for (int x=0; x<c; x++) cout << "=";
                        for (int x=c; x<50; x++) cout << " ";
                        cout << "]\r" << flush;
                    }
                    double x = hWidthX->GetXaxis()->GetBinCenter(bx);
                    double y = hWidthX->GetYaxis()->GetBinCenter(by);
                    double z = hWidthX->GetZaxis()->GetBinCenter(bz);
                    double xBinWidth = hWidthX->GetXaxis()->GetBinWidth(bx);
                    double yBinWidth = hWidthX->GetYaxis()->GetBinWidth(by);
                    double zBinWidth = hWidthX->GetZaxis()->GetBinWidth(bz);
                    vector<double> point;
                    point.push_back(x);
                    point.push_back(y);
                    point.push_back(z);
                    BinLeaf* leaf = getLeaf(point);
                    if(leaf->getWidth(0)<=xBinWidth && leaf->getWidth(1)<=yBinWidth && leaf->getWidth(2)<=zBinWidth)
                    {
                        hWidthX->SetBinContent(bx,by,bz, leaf->getWidth(0));
                        hWidthY->SetBinContent(bx,by,bz, leaf->getWidth(1));
                        hWidthZ->SetBinContent(bx,by,bz, leaf->getWidth(2));
                        hWidthX->SetBinError(bx,by, bz, 0.);
                        hWidthY->SetBinError(bx,by, bz, 0.);
                        hWidthZ->SetBinError(bx,by, bz, 0.);
                        continue;
                    }
                    //cout<<"Computing width for ("<<x<<","<<y<<")\n";
                    vector<BinLeaf*>::iterator itLeaf = neighborLeaves.begin();
                    vector<BinLeaf*>::iterator itELeaf = neighborLeaves.end();
                    double sumw = 0.;
                    double sumwx = 0.;
                    double sumwy = 0.;
                    double sumwz = 0.;
                    //cout<<" "<<neighborLeaves.size()<<" neighbor leaves\n";
                    for(;itLeaf!=itELeaf;++itLeaf)
                    {
                        double xi = (*itLeaf)->getCenter(0);
                        double yi = (*itLeaf)->getCenter(1);
                        double zi = (*itLeaf)->getCenter(2);
                        double dx = fabs(xi-x);
                        double dy = fabs(yi-y);
                        double dz = fabs(zi-z);
                        double wxi = (*itLeaf)->getWidth(0);
                        double wyi = (*itLeaf)->getWidth(1);
                        double wzi = (*itLeaf)->getWidth(2);
                        if(dx<0.001*wxi) dx = 0.001*wxi;
                        if(dy<0.001*wyi) dy = 0.001*wyi;
                        if(dz<0.001*wzi) dz = 0.001*wzi;
                        double dr2 = dx*dx+dy*dy+dz*dz;
                        double dr = dr2;
                        sumw += 1./dr;
                        sumwx += wxi/dr;
                        sumwy += wyi/dr;
                        sumwz += wzi/dr;
                        //cout<<"  Neighbor leaf dx="<<dx<<",dy="<<dy<<",w="<<1./dr2<<"\n";
                    }
                    double widthx = sumwx/sumw;
                    double widthy = sumwy/sumw;
                    double widthz = sumwz/sumw;
                    hWidthX->SetBinContent(bx,by,bz,widthx);
                    hWidthY->SetBinContent(bx,by,bz,widthy);
                    hWidthZ->SetBinContent(bx,by,bz,widthz);
                    hWidthX->SetBinError(bx,by,bz,0.);
                    hWidthY->SetBinError(bx,by,bz,0.);
                    hWidthZ->SetBinError(bx,by,bz,0.);
                }
            }
        }
        widths.push_back(hWidthX);
        widths.push_back(hWidthY);
        widths.push_back(hWidthZ);
    }
    cout << "[INFO]   "<< setw(3) << 100 << "% [";
    for (int x=0; x<50; x++) cout << "=";
    cout << "]" << endl;
    return widths;
}

/*****************************************************************/
vector<TH1*> BinTree::fillWidths(const TH1* widthTemplate)
/*****************************************************************/
{
    if(getNLeaves()<500)
    {
        return fillWidthsLowStat(widthTemplate); // use all bins to evaluate the width (gives more smooth transitions)
    }
    else
    {
        return fillWidthsHighStat(widthTemplate); // Use only neighbors bins to evaluate the width
    }
}





