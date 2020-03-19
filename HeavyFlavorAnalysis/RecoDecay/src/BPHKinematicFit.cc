/*
 *  See header file for a description of this class.
 *
 *  \author Paolo Ronchese INFN Padova
 *
 */

//-----------------------
// This Class' Header --
//-----------------------
#include "HeavyFlavorAnalysis/RecoDecay/interface/BPHKinematicFit.h"

//-------------------------------
// Collaborating Class Headers --
//-------------------------------
#include "HeavyFlavorAnalysis/RecoDecay/interface/BPHRecoCandidate.h"
#include "RecoVertex/KinematicFitPrimitives/interface/KinematicParticleFactoryFromTransientTrack.h"
#include "RecoVertex/KinematicFitPrimitives/interface/RefCountedKinematicParticle.h"
#include "RecoVertex/KinematicFit/interface/KinematicParticleVertexFitter.h"
#include "RecoVertex/KinematicFit/interface/KinematicConstrainedVertexFitter.h"
#include "RecoVertex/KinematicFit/interface/KinematicParticleFitter.h"
#include "RecoVertex/KinematicFit/interface/MassKinematicConstraint.h"
#include "RecoVertex/KinematicFit/interface/TwoTrackMassKinematicConstraint.h"
#include "RecoVertex/KinematicFit/interface/MultiTrackMassKinematicConstraint.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

//---------------
// C++ Headers --
//---------------
using namespace std;

//-------------------
// Initializations --
//-------------------

//----------------
// Constructors --
//----------------
BPHKinematicFit::BPHKinematicFit()
    : BPHDecayVertex(nullptr),
      massConst(-1.0),
      massSigma(-1.0),
      oldKPs(true),
      oldFit(true),
      oldMom(true),
      kinTree(nullptr) {}

BPHKinematicFit::BPHKinematicFit(const BPHKinematicFit* ptr)
    : BPHDecayVertex(ptr, nullptr),
      massConst(-1.0),
      massSigma(-1.0),
      oldKPs(true),
      oldFit(true),
      oldMom(true),
      kinTree(nullptr) {
  map<const reco::Candidate*, const reco::Candidate*> iMap;
  const vector<const reco::Candidate*>& daug = daughters();
  const vector<Component>& list = ptr->componentList();
  int i;
  int n = daug.size();
  for (i = 0; i < n; ++i) {
    const reco::Candidate* cand = daug[i];
    iMap[originalReco(cand)] = cand;
  }
  for (i = 0; i < n; ++i) {
    const Component& c = list[i];
    dMSig[iMap[c.cand]] = c.msig;
  }
  const vector<BPHRecoConstCandPtr>& dComp = daughComp();
  int j;
  int m = dComp.size();
  for (j = 0; j < m; ++j) {
    const map<const reco::Candidate*, double>& dMap = dComp[j]->dMSig;
    dMSig.insert(dMap.begin(), dMap.end());
  }
}

//--------------
// Destructor --
//--------------
BPHKinematicFit::~BPHKinematicFit() {}

//--------------
// Operations --
//--------------
/// apply a mass constraint
void BPHKinematicFit::setConstraint(double mass, double sigma) {
  oldFit = oldMom = true;
  massConst = mass;
  massSigma = sigma;
  return;
}

/// retrieve the constraint
double BPHKinematicFit::constrMass() const { return massConst; }

double BPHKinematicFit::constrSigma() const { return massSigma; }

/// get kinematic particles
const vector<RefCountedKinematicParticle>& BPHKinematicFit::kinParticles() const {
  if (oldKPs)
    buildParticles();
  return allParticles;
}

vector<RefCountedKinematicParticle> BPHKinematicFit::kinParticles(const vector<string>& names) const {
  if (oldKPs)
    buildParticles();
  const vector<const reco::Candidate*>& daugs = daughFull();
  vector<RefCountedKinematicParticle> plist;
  if (allParticles.size() != daugs.size())
    return plist;
  set<RefCountedKinematicParticle> pset;
  int i;
  int n = names.size();
  int m = daugs.size();
  plist.reserve(m);
  for (i = 0; i < n; ++i) {
    const string& pname = names[i];
    if (pname == "*") {
      int j = m;
      while (j--) {
        RefCountedKinematicParticle& kp = allParticles[j];
        if (pset.find(kp) != pset.end())
          continue;
        plist.push_back(kp);
        pset.insert(kp);
      }
      break;
    }
    map<const reco::Candidate*, RefCountedKinematicParticle>::const_iterator iter = kinMap.find(getDaug(pname));
    map<const reco::Candidate*, RefCountedKinematicParticle>::const_iterator iend = kinMap.end();
    if (iter != iend) {
      const RefCountedKinematicParticle& kp = iter->second;
      if (pset.find(kp) != pset.end())
        continue;
      plist.push_back(kp);
      pset.insert(kp);
    } else {
      edm::LogPrint("ParticleNotFound") << "BPHKinematicFit::kinParticles: " << pname << " not found";
    }
  }
  return plist;
}

/// perform the kinematic fit and get the result
const RefCountedKinematicTree& BPHKinematicFit::kinematicTree() const {
  if (oldFit)
    return kinematicTree("", massConst, massSigma);
  return kinTree;
}

const RefCountedKinematicTree& BPHKinematicFit::kinematicTree(const string& name, double mass, double sigma) const {
  if (mass < 0)
    return kinematicTree(name);
  if (sigma < 0)
    return kinematicTree(name, mass);
  ParticleMass mc = mass;
  MassKinematicConstraint kinConst(mc, sigma);
  return kinematicTree(name, &kinConst);
}

const RefCountedKinematicTree& BPHKinematicFit::kinematicTree(const string& name, double mass) const {
  if (mass < 0)
    return kinematicTree(name);
  int nn = daughFull().size();
  ParticleMass mc = mass;
  if (nn == 2) {
    TwoTrackMassKinematicConstraint kinConst(mc);
    return kinematicTree(name, &kinConst);
  } else {
    MultiTrackMassKinematicConstraint kinConst(mc, nn);
    return kinematicTree(name, &kinConst);
  }
}

const RefCountedKinematicTree& BPHKinematicFit::kinematicTree(const string& name) const {
  KinematicConstraint* kc = nullptr;
  return kinematicTree(name, kc);
}

const RefCountedKinematicTree& BPHKinematicFit::kinematicTree(const string& name, KinematicConstraint* kc) const {
  kinTree = RefCountedKinematicTree(nullptr);
  oldFit = false;
  kinParticles();
  if (allParticles.size() != daughFull().size())
    return kinTree;
  vector<RefCountedKinematicParticle> kComp;
  vector<RefCountedKinematicParticle> kTail;
  if (!name.empty()) {
    const BPHRecoCandidate* comp = getComp(name).get();
    if (comp == nullptr) {
      edm::LogPrint("ParticleNotFound") << "BPHKinematicFit::kinematicTree: " << name << " daughter not found";
      return kinTree;
    }
    const vector<string>& names = comp->daugNames();
    int ns;
    int nn = ns = names.size();
    vector<string> nfull(nn + 1);
    nfull[nn] = "*";
    while (nn--)
      nfull[nn] = name + "/" + names[nn];
    vector<RefCountedKinematicParticle> kPart = kinParticles(nfull);
    vector<RefCountedKinematicParticle>::const_iterator iter = kPart.begin();
    vector<RefCountedKinematicParticle>::const_iterator imid = iter + ns;
    vector<RefCountedKinematicParticle>::const_iterator iend = kPart.end();
    kComp.insert(kComp.end(), iter, imid);
    kTail.insert(kTail.end(), imid, iend);
  } else {
    kComp = allParticles;
  }
  try {
    KinematicParticleVertexFitter vtxFitter;
    RefCountedKinematicTree compTree = vtxFitter.fit(kComp);
    if (compTree->isEmpty())
      return kinTree;
    if (kc != nullptr) {
    KinematicParticleFitter kinFitter;
      compTree = kinFitter.fit(kc, compTree);
      if (compTree->isEmpty())
        return kinTree;
    }
    compTree->movePointerToTheTop();
    if (!kTail.empty()) {
      RefCountedKinematicParticle compPart = compTree->currentParticle();
      if (!compPart->currentState().isValid())
        return kinTree;
      kTail.push_back(compPart);
      kinTree = vtxFitter.fit(kTail);
    } else {
      kinTree = compTree;
    }
  } catch (std::exception const&) {
    edm::LogPrint("FitFailed") << "BPHKinematicFit::kinematicTree: "
                               << "kin fit reset";
    kinTree = RefCountedKinematicTree(nullptr);
  }
  return kinTree;
}

const RefCountedKinematicTree& BPHKinematicFit::kinematicTree(const string& name,
                                                              MultiTrackKinematicConstraint* kc) const {
  kinTree = RefCountedKinematicTree(nullptr);
  oldFit = false;
  kinParticles();
  if (allParticles.size() != daughFull().size())
    return kinTree;
  vector<string> nfull;
  if (!name.empty()) {
    const BPHRecoCandidate* comp = getComp(name).get();
    if (comp == nullptr) {
      edm::LogPrint("ParticleNotFound") << "BPHKinematicFit::kinematicTree: " << name << " daughter not found";
      return kinTree;
    }
    const vector<string>& names = comp->daugNames();
    int nn = names.size();
    nfull.resize(nn + 1);
    nfull[nn] = "*";
    while (nn--)
      nfull[nn] = name + "/" + names[nn];
  } else {
    nfull.push_back("*");
  }
  try {
    KinematicConstrainedVertexFitter cvf;
    kinTree = cvf.fit(kinParticles(nfull), kc);
  } catch (std::exception const&) {
    edm::LogPrint("FitFailed") << "BPHKinematicFit::kinematicTree: "
                               << "kin fit reset";
    kinTree = RefCountedKinematicTree(nullptr);
  }
  return kinTree;
}

/// reset the kinematic fit
void BPHKinematicFit::resetKinematicFit() const {
  oldKPs = oldFit = oldMom = true;
  return;
}

/// get fit status
bool BPHKinematicFit::isEmpty() const {
  kinematicTree();
  if (kinTree.get() == nullptr)
    return true;
  return kinTree->isEmpty();
}

bool BPHKinematicFit::isValidFit() const {
  const RefCountedKinematicParticle kPart = topParticle();
  if (kPart.get() == nullptr)
    return false;
  return kPart->currentState().isValid();
}

/// get current particle
const RefCountedKinematicParticle BPHKinematicFit::currentParticle() const {
  if (isEmpty())
    return RefCountedKinematicParticle(nullptr);
  return kinTree->currentParticle();
}

const RefCountedKinematicVertex BPHKinematicFit::currentDecayVertex() const {
  if (isEmpty())
    return RefCountedKinematicVertex(nullptr);
  return kinTree->currentDecayVertex();
}

/// get top particle
const RefCountedKinematicParticle BPHKinematicFit::topParticle() const {
  if (isEmpty())
    return RefCountedKinematicParticle(nullptr);
  return kinTree->topParticle();
}

const RefCountedKinematicVertex BPHKinematicFit::topDecayVertex() const {
  if (isEmpty())
    return RefCountedKinematicVertex(nullptr);
  kinTree->movePointerToTheTop();
  return kinTree->currentDecayVertex();
}

ParticleMass BPHKinematicFit::mass() const {
  const RefCountedKinematicParticle kPart = topParticle();
  if (kPart.get() == nullptr)
    return -1.0;
  const KinematicState kStat = kPart->currentState();
  if (kStat.isValid())
    return kStat.mass();
  return -1.0;
}

/// compute total momentum after the fit
const math::XYZTLorentzVector& BPHKinematicFit::p4() const {
  if (oldMom)
    fitMomentum();
  return totalMomentum;
}

/// retrieve particle mass sigma
double BPHKinematicFit::getMassSigma(const reco::Candidate* cand) const {
  map<const reco::Candidate*, double>::const_iterator iter = dMSig.find(cand);
  return (iter != dMSig.end() ? iter->second : -1);
}

/// add a simple particle giving it a name
/// particles are cloned, eventually specifying a different mass
/// and a sigma
void BPHKinematicFit::addK(const string& name, const reco::Candidate* daug, double mass, double sigma) {
  addK(name, daug, "cfhpmig", mass, sigma);
  return;
}

/// add a simple particle and specify a criterion to search for
/// the associated track
void BPHKinematicFit::addK(
    const string& name, const reco::Candidate* daug, const string& searchList, double mass, double sigma) {
  addV(name, daug, searchList, mass);
  dMSig[daughters().back()] = sigma;
  return;
}

/// add a previously reconstructed particle giving it a name
void BPHKinematicFit::addK(const string& name, const BPHRecoConstCandPtr& comp) {
  addV(name, comp);
  const map<const reco::Candidate*, double>& dMap = comp->dMSig;
  dMSig.insert(dMap.begin(), dMap.end());
  return;
}

// utility function used to cash reconstruction results
void BPHKinematicFit::setNotUpdated() const {
  BPHDecayVertex::setNotUpdated();
  resetKinematicFit();
  return;
}

// build kin particles, perform the fit and compute the total momentum
void BPHKinematicFit::buildParticles() const {
  kinMap.clear();
  kCDMap.clear();
  allParticles.clear();
  allParticles.reserve(daughFull().size());
  addParticles(allParticles, kinMap, kCDMap);
  oldKPs = false;
  return;
}

void BPHKinematicFit::addParticles(vector<RefCountedKinematicParticle>& kl,
                                   map<const reco::Candidate*, RefCountedKinematicParticle>& km,
                                   map<const BPHRecoCandidate*, RefCountedKinematicParticle>& cm) const {
  const vector<const reco::Candidate*>& daug = daughters();
  KinematicParticleFactoryFromTransientTrack pFactory;
  int n = daug.size();
  float chi = 0.0;
  float ndf = 0.0;
  while (n--) {
    const reco::Candidate* cand = daug[n];
    ParticleMass mass = cand->mass();
    float sigma = dMSig.find(cand)->second;
    if (sigma < 0)
      sigma = 1.0e-7;
    reco::TransientTrack* tt = getTransientTrack(cand);
    if (tt != nullptr)
      kl.push_back(km[cand] = pFactory.particle(*tt, mass, chi, ndf, sigma));
  }
  const vector<BPHRecoConstCandPtr>& comp = daughComp();
  int m = comp.size();
  while (m--) {
    const BPHRecoCandidate* cptr = comp[m].get();
    cptr->addParticles(kl, km, cm);
  }
  return;
}

void BPHKinematicFit::fitMomentum() const {
  if (isValidFit()) {
    const KinematicState& ks = topParticle()->currentState();
    GlobalVector tm = ks.globalMomentum();
    double x = tm.x();
    double y = tm.y();
    double z = tm.z();
    double m = ks.mass();
    double e = sqrt((x * x) + (y * y) + (z * z) + (m * m));
    totalMomentum.SetPxPyPzE(x, y, z, e);
  } else {
    edm::LogPrint("FitNotFound") << "BPHKinematicFit::fitMomentum: "
                                 << "simple momentum sum computed";
    math::XYZTLorentzVector tm;
    const vector<const reco::Candidate*>& daug = daughters();
    int n = daug.size();
    while (n--)
      tm += daug[n]->p4();
    const vector<BPHRecoConstCandPtr>& comp = daughComp();
    int m = comp.size();
    while (m--)
      tm += comp[m]->p4();
    totalMomentum = tm;
  }
  oldMom = false;
  return;
}
