/** \class HTauTauNtuplizer
 *
 *  No description available.
 *
 *  $Date: 2014/10/31 10:08:19 $
 *  $Revision: 1.00 $
 *  \author G. Ortona (LLR)
 */

// system include files
#include <memory>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <TNtuple.h>
//#include <XYZTLorentzVector.h>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include <FWCore/Framework/interface/ESHandle.h>
#include <FWCore/Framework/interface/LuminosityBlock.h>
#include <FWCore/ParameterSet/interface/ParameterSet.h>
#include <FWCore/Common/interface/TriggerNames.h>

#include <DataFormats/PatCandidates/interface/Muon.h>
#include <DataFormats/PatCandidates/interface/MET.h>
#include "DataFormats/PatCandidates/interface/Jet.h"
#include <DataFormats/PatCandidates/interface/GenericParticle.h>
#include "DataFormats/VertexReco/interface/Vertex.h"
#include <DataFormats/Common/interface/View.h>
#include <DataFormats/Candidate/interface/Candidate.h>
#include <DataFormats/PatCandidates/interface/CompositeCandidate.h>
#include <DataFormats/PatCandidates/interface/Electron.h>
#include <DataFormats/METReco/interface/PFMET.h>
#include <DataFormats/METReco/interface/PFMETCollection.h>
#include <DataFormats/JetReco/interface/PFJet.h>
#include <DataFormats/JetReco/interface/PFJetCollection.h>
#include <DataFormats/Math/interface/LorentzVector.h>
#include <DataFormats/VertexReco/interface/Vertex.h>
#include <DataFormats/Common/interface/MergeableCounter.h>
#include "DataFormats/Math/interface/deltaR.h"

#include "DataFormats/HLTReco/interface/TriggerObject.h"
#include "DataFormats/HLTReco/interface/TriggerEvent.h"
#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/PatCandidates/interface/TriggerObjectStandAlone.h"
#include "HLTrigger/HLTcore/interface/HLTConfigProvider.h"

#include "SimDataFormats/PileupSummaryInfo/interface/PileupSummaryInfo.h"
#include "SimDataFormats/GeneratorProducts/interface/GenFilterInfo.h"
#include <CommonTools/UtilAlgos/interface/TFileService.h>

#include <Muon/MuonAnalysisTools/interface/MuonEffectiveArea.h>

#include <LLRHiggsTauTau/NtupleProducer/interface/CutSet.h>
#include <LLRHiggsTauTau/NtupleProducer/interface/LeptonIsoHelper.h>
#include <LLRHiggsTauTau/NtupleProducer/interface/DaughterDataHelpers.h>
//#include <LLRHiggsTauTau/NtupleProducer/interface/FinalStates.h>
#include <LLRHiggsTauTau/NtupleProducer/interface/MCHistoryTools.h>
#include <LLRHiggsTauTau/NtupleProducer/interface/PUReweight.h>
//#include <LLRHiggsTauTau/NtupleProducer/interface/VBFCandidateJetSelector.h>
//#include <LLRHiggsTauTau/NtupleProducer/interface/bitops.h>
//#include <LLRHiggsTauTau/NtupleProducer/interface/Fisher.h>
//#include <LLRHiggsTauTau/NtupleProducer/interface/HTauTauConfigHelper.h>
//#include "HZZ4lNtupleFactory.h"
#include <LLRHiggsTauTau/NtupleProducer/interface/PhotonFwd.h>
#include "LLRHiggsTauTau/NtupleProducer/interface/triggerhelper.h"
#include "LLRHiggsTauTau/NtupleProducer/interface/PUReweight.h"
//#include "LLRHiggsTauTau/NtupleProducer/Utils/OfflineProducerHelper.h"
#include "LLRHiggsTauTau/NtupleProducer/interface/ParticleType.h"


//#include "TLorentzVector.h"

 namespace {
//   bool writePhotons = false;  // Write photons in the tree. 
   bool writeJets = true;     // Write jets in the tree. 
   bool writeSoftLep = false;
   bool DEBUG = false;
 }

using namespace std;
using namespace edm;
using namespace reco;

static bool ComparePairs(pat::CompositeCandidate i, pat::CompositeCandidate j){
  //sto sbagliando! la cosa dovrebbe essere disponibile per i vari criteri, non per la sequenza 
  //First criteria: OS<SS
  if ( j.charge()==0 && i.charge()!=0) return false;
  else if ( i.charge()==0 && j.charge()!=0) return true;

  //Second criteria: legs pt
  if(i.daughter(0)->pt()+i.daughter(1)->pt()<j.daughter(0)->pt()+j.daughter(1)->pt()) return false;
  if(i.daughter(0)->pt()+i.daughter(1)->pt()>j.daughter(0)->pt()+j.daughter(1)->pt()) return true; 
  
  //Protection for duplicated taus, damn it!
  int iType=0,jType=0;
  for(int idau=0;idau<2;idau++){
    if(i.daughter(idau)->isElectron())iType+=0;
    else if(i.daughter(idau)->isMuon())iType+=1;
    else iType+=2;
    if(j.daughter(idau)->isElectron())jType+=0;
    else if(j.daughter(idau)->isMuon())jType+=1;
    else jType+=2;
  }
  
  return (iType<jType);
  
  //I need a criteria for where to put pairs wo taus and how to deal with tautau candidates
  
  //third criteria: are there taus?
  int ilegtau=-1,jlegtau=-1;
  if(!i.daughter(0)->isMuon() && !i.daughter(0)->isElectron())ilegtau=0;
  if(!j.daughter(0)->isMuon() && !j.daughter(0)->isElectron())jlegtau=0;
 
  if(!i.daughter(1)->isMuon() && !i.daughter(1)->isElectron()){
    if(ilegtau==-1 || i.daughter(1)->pt()>i.daughter(0)->pt())ilegtau=1;}
  if(!j.daughter(1)->isMuon() && !j.daughter(1)->isElectron()){
    if(ilegtau==-1 || i.daughter(1)->pt()>i.daughter(0)->pt())jlegtau=1;}

  
  if(ilegtau==-1 && jlegtau>-1) return false; //i has no tau leptons, j has
  else if(ilegtau==-1 && jlegtau == -1) return true; //no tau leptons in neither pair, leave as it is
  
  //fourth criteria: Iso x Legpt
  if(userdatahelpers::getUserFloat(i.daughter(ilegtau),"byCombinedIsolationDeltaBetaCorrRaw3Hits")*i.daughter(fabs(ilegtau-1))->pt()>userdatahelpers::getUserFloat(j.daughter(jlegtau),"byCombinedIsolationDeltaBetaCorrRaw3Hits")*j.daughter(fabs(jlegtau-1))->pt()) return false;
  
  //fifth criteria: Iso (cut based)
  if(userdatahelpers::getUserFloat(i.daughter(ilegtau),"combRelIsoPF")>userdatahelpers::getUserFloat(j.daughter(jlegtau),"combRelIsoPF")) return false;
  
  //sixth criteria: ISO (MVA)
  if(userdatahelpers::getUserFloat(i.daughter(ilegtau),"byCombinedIsolationDeltaBetaCorrRaw3Hits")*userdatahelpers::getUserFloat(i.daughter(fabs(ilegtau-1)),"combRelIsoPF")>userdatahelpers::getUserFloat(j.daughter(jlegtau),"byCombinedIsolationDeltaBetaCorrRaw3Hits")*userdatahelpers::getUserFloat(j.daughter(fabs(jlegtau-1)),"combRelIsoPF")) return false;

  return true;
}

// class declaration

class HTauTauNtuplizer : public edm::EDAnalyzer {
 public:
  /// Constructor
  explicit HTauTauNtuplizer(const edm::ParameterSet&);
    
  /// Destructor
  ~HTauTauNtuplizer();  

 private:
  //----edm control---
  virtual void beginJob() ;
  virtual void analyze(const edm::Event&, const edm::EventSetup&);
  virtual void endJob() ;
  virtual void beginRun(edm::Run const&, edm::EventSetup const&);
  virtual void endRun(edm::Run const&, edm::EventSetup const&);
  virtual void beginLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&);
  virtual void endLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&);
  //void InitializeBranches();
  //void InitializeVariables();
  void Initialize(); 
  Int_t FindCandIndex(const reco::Candidate&, Int_t iCand);
  //----To implement here-----
  //virtual void FillCandidate(const pat::CompositeCandidate& higgs, bool evtPass, const edm::Event&, const Int_t CRflag);
  //virtual void FillPhoton(const pat::Photon& photon);
  int FillJet(const edm::View<pat::Jet>* jet);
  void FillSoftLeptons(const edm::View<reco::Candidate> *dauhandler, const edm::Event& event, bool theFSR);
  void FillbQuarks(const edm::Event&);
  bool CompareLegs(const reco::Candidate *, const reco::Candidate *);
  //bool ComparePairs(pat::CompositeCandidate i, pat::CompositeCandidate j);

  // ----------member data ---------------------------
  //Configs
  int theChannel;
  std::string theCandLabel;
  TString theFileName;
  bool theFSR;
  Bool_t theisMC;
  //Trigger
  vector<int> indexOfPath;
  vector<string> foundPaths;
  //Int_t nFoundPaths;
  //edm::InputTag triggerResultsLabel;
  edm::InputTag processName;
  //edm::InputTag triggerSet;

  HLTConfigProvider hltConfig_;
  //Output Objects
  TTree *myTree;//->See from ntuplefactory in zz4l
  TH1F *hCounter;

  PUReweight reweight;
  edm::EDGetTokenT<pat::TriggerObjectStandAloneCollection> triggerObjects_;
  edm::EDGetTokenT<edm::TriggerResults> triggerBits_;

  //flags
  static const int nOutVars =14;
  bool applyTrigger;    // Only events passing trigger
  bool applySkim;       //  "     "      "     skim
  bool skipEmptyEvents; // Skip events whith no candidate in the collection
  //PUReweight reweight;

  //counters
  Int_t Nevt_Gen;
  Int_t Nevt_PassTrigger;
  Int_t Npairs;

  //Event Output variables
  Int_t _indexevents;
  Int_t _runNumber;
  Int_t _lumi;
  Int_t _triggerbit;
  Int_t _metfilterbit;
  Float_t _met;
  Float_t _metphi;
  Float_t _MC_weight;
  Int_t _npv;
  Int_t _npu;
  Float_t _PUReweight;
  Float_t _rho;
  
  //Leptons
  //std::vector<TLorentzVector> _mothers;
  std::vector<Float_t> _mothers_px;
  std::vector<Float_t> _mothers_py;
  std::vector<Float_t> _mothers_pz;
  std::vector<Float_t> _mothers_e;
  
  //std::vector<TLorentzVector> _daughters;
  std::vector<Float_t> _daughters_px;
  std::vector<Float_t> _daughters_py;
  std::vector<Float_t> _daughters_pz;
  std::vector<Float_t> _daughters_e;

  std::vector<const reco::Candidate*> _softLeptons;
  
  //std::vector<TLorentzVector> _bquarks;
  std::vector<Float_t> _bquarks_px;
  std::vector<Float_t> _bquarks_py;
  std::vector<Float_t> _bquarks_pz;
  std::vector<Float_t> _bquarks_e;
  std::vector<Int_t> _bquarks_pdg;
  
  //std::vector<math::XYZTLorentzVector> _daughter2;

  //Mothers output variables
  std::vector<Int_t> _indexDau1;
  std::vector<Int_t> _indexDau2;
  std::vector<Int_t> _genDaughters;
  std::vector<Bool_t> _isOSCand;
  std::vector<Float_t> _SVmass;
  std::vector<Float_t> _metx;
  std::vector<Float_t> _mety;
  std::vector<Float_t> _metCov00;
  std::vector<Float_t> _metCov01;
  std::vector<Float_t> _metCov10;
  std::vector<Float_t> _metCov11;
  std::vector<Float_t> _metSignif;
  std::vector<Float_t> _bmotmass;
   
  //Leptons variables
  std::vector<Int_t> _pdgdau;
  std::vector<Int_t> _particleType;//0=muon, 1=e, 2=tau
  std::vector<Float_t> _combreliso;
  std::vector<Float_t> _discriminator;//BDT for ele, discriminator for tau, muonID for muons (bit 0 loose, 1 soft , 2 medium, 3 tight)
  std::vector<Float_t> _dxy;
  std::vector<Float_t> _dz;
  std::vector<bool> _daughters_iseleBDT; //isBDT for ele
  std::vector<bool> _daughters_iseleCUT; //isBDT for ele
  std::vector<Int_t> _decayType;//for taus only
  std::vector<Float_t> _daughters_IetaIeta;
  std::vector<Float_t> _daughters_deltaPhiSuperClusterTrackAtVtx;
  std::vector<Float_t> _daughters_depositR03_tracker;
  std::vector<Float_t> _daughters_depositR03_ecal;
  std::vector<Float_t> _daughters_depositR03_hcal;
  std::vector<Int_t> _daughters_decayModeFindingOldDMs;
  std::vector<Int_t> _daughters_decayModeFindingNewDMs;
  std::vector<Int_t> _daughters_byLooseCombinedIsolationDeltaBetaCorr3Hits;
  std::vector<Int_t> _daughters_byMediumCombinedIsolationDeltaBetaCorr3Hits;
  std::vector<Int_t> _daughters_byTightCombinedIsolationDeltaBetaCorr3Hits;
  std::vector<Float_t> _daughters_byCombinedIsolationDeltaBetaCorrRaw3Hits;
  std::vector<Float_t> _daughters_chargedIsoPtSum;
  std::vector<Float_t> _daughters_neutralIsoPtSum;
  std::vector<Float_t> _daughters_puCorrPtSum;
  std::vector<Int_t> _daughters_againstMuonLoose3;
  std::vector<Int_t> _daughters_againstMuonTight3;
  std::vector<Int_t> _daughters_againstElectronVLooseMVA5;
  std::vector<Int_t> _daughters_againstElectronLooseMVA5;
  std::vector<Int_t> _daughters_againstElectronMediumMVA5;
  std::vector<Int_t> _daughters_LFtrigger;
  std::vector<Int_t> _daughters_L3trigger;
  std::vector<Int_t> _daughters_FilterFired;
  std::vector<bool> _daughters_isGoodTriggerType;
  std::vector<Int_t> _daughters_L3FilterFired;
  std::vector<Int_t> _daughters_L3FilterFiredLast;

  //Jets variables
  Int_t _numberOfJets;
  //std::vector<TLorentzVector> _jets;
  std::vector<Float_t> _jets_px;
  std::vector<Float_t> _jets_py;
  std::vector<Float_t> _jets_pz;
  std::vector<Float_t> _jets_e;
  std::vector<Float_t> _jets_PUJetID;
  std::vector<Int_t> _jets_Flavour;
  std::vector<Float_t> _bdiscr;
  std::vector<Float_t> _bdiscr2;
  
  //genH
  std::vector<Float_t> _genH_px;
  std::vector<Float_t> _genH_py;
  std::vector<Float_t> _genH_pz;
  std::vector<Float_t> _genH_e;
};

// ----Constructor and Destructor -----
HTauTauNtuplizer::HTauTauNtuplizer(const edm::ParameterSet& pset) : reweight(),
  triggerObjects_(consumes<pat::TriggerObjectStandAloneCollection>(pset.getParameter<edm::InputTag>("triggerSet"))),
  triggerBits_(consumes<edm::TriggerResults>(pset.getParameter<edm::InputTag>("triggerResultsLabel")))

 {
  theCandLabel = pset.getUntrackedParameter<string>("CandCollection");
  //theChannel = myHelper.channel();
  theFileName = pset.getUntrackedParameter<string>("fileName");
  skipEmptyEvents = pset.getParameter<bool>("skipEmptyEvents");
  theFSR = pset.getParameter<bool>("applyFSR");
  theisMC = pset.getParameter<bool>("IsMC");
  //writeBestCandOnly = pset.getParameter<bool>("onlyBestCandidate");
  //sampleName = pset.getParameter<string>("sampleName");
  Nevt_Gen=0;
  Nevt_PassTrigger = 0;
  Npairs=0;
  //triggerResultsLabel = InputTag("TriggerResults","","HLT");
  processName= pset.getParameter<edm::InputTag>("triggerResultsLabel");
  //triggerSet= pset.getParameter<edm::InputTag>("triggerSet");

  Initialize();
}

HTauTauNtuplizer::~HTauTauNtuplizer(){}
//

void HTauTauNtuplizer::Initialize(){
  //_mothers.clear();
  _mothers_px.clear();
  _mothers_py.clear();
  _mothers_pz.clear();
  _mothers_e.clear();
  
  //_daughters.clear();
  _daughters_px.clear();
  _daughters_py.clear();
  _daughters_pz.clear();
  _daughters_e.clear();
  _daughters_IetaIeta.clear();
  _daughters_deltaPhiSuperClusterTrackAtVtx.clear();
  _daughters_depositR03_tracker.clear();  
  _daughters_depositR03_ecal.clear();  
  _daughters_depositR03_hcal.clear();  
  _daughters_decayModeFindingOldDMs.clear();
  _daughters_decayModeFindingNewDMs.clear();
  _daughters_byLooseCombinedIsolationDeltaBetaCorr3Hits.clear();
  _daughters_byMediumCombinedIsolationDeltaBetaCorr3Hits.clear();
  _daughters_byTightCombinedIsolationDeltaBetaCorr3Hits.clear();
  _daughters_byCombinedIsolationDeltaBetaCorrRaw3Hits.clear();
  _daughters_chargedIsoPtSum.clear();
  _daughters_neutralIsoPtSum.clear();
  _daughters_puCorrPtSum.clear();
  _daughters_againstMuonLoose3.clear();
  _daughters_againstMuonTight3.clear();
  _daughters_againstElectronVLooseMVA5.clear();
  _daughters_againstElectronLooseMVA5.clear();
  _daughters_againstElectronMediumMVA5.clear();
  _daughters_LFtrigger.clear();
  _daughters_L3trigger.clear();
  _daughters_FilterFired.clear();
  _daughters_isGoodTriggerType.clear();
  _daughters_L3FilterFired.clear();
  _daughters_L3FilterFiredLast.clear();
  _daughters_iseleBDT.clear();
  _daughters_iseleCUT.clear();
  //_daughter2.clear();
  _softLeptons.clear();
  _genDaughters.clear();
  
  //_bquarks.clear();
  _bquarks_px.clear();
  _bquarks_py.clear();
  _bquarks_pz.clear();
  _bquarks_e.clear();
  _bquarks_pdg.clear();
  _bmotmass.clear();
  
  _indexDau1.clear();
  _indexDau2.clear();
  _pdgdau.clear();
  _SVmass.clear();
  _isOSCand.clear();
  _metx.clear();
  _mety.clear();
  _metCov00.clear();
  _metCov01.clear();
  _metCov10.clear();
  _metCov11.clear();
  _metSignif.clear();
  _particleType.clear();
  _discriminator.clear();
  _dxy.clear();
  _dz.clear();
  _decayType.clear();
  _combreliso.clear();
  _indexevents=0;
  _runNumber=0;
  _lumi=0;
  _triggerbit=0;
  _metfilterbit=0;
  _met=0;
  _metphi=0.;
  _MC_weight=0.;
  _npv=0;
  _npu=0;
  _PUReweight=0.;
  _rho=0;

//  _jets.clear();
  _jets_px.clear();
  _jets_py.clear();
  _jets_pz.clear();
  _jets_e.clear();
  _jets_PUJetID.clear();
  _jets_Flavour.clear();
  _numberOfJets=0;
  _bdiscr.clear();
  _bdiscr2.clear();
  
  _genH_px.clear();
  _genH_py.clear();
  _genH_pz.clear();
  _genH_e.clear();
}

void HTauTauNtuplizer::beginJob(){
  edm::Service<TFileService> fs;
  myTree = fs->make<TTree>("HTauTauTree","HTauTauTree");
  hCounter = fs->make<TH1F>("Counters","Counters",3,0,3);

  //Branches
  myTree->Branch("EventNumber",&_indexevents,"EventNumber/I");
  myTree->Branch("RunNumber",&_runNumber,"RunNumber/I");
  myTree->Branch("lumi",&_lumi,"lumi/I");
  myTree->Branch("triggerbit",&_triggerbit,"triggerbit/I");
  myTree->Branch("metfilterbit",&_metfilterbit,"metfilterbit/I");
  myTree->Branch("met",&_met,"met/F");
  myTree->Branch("metphi",&_metphi,"metphi/F");  
  myTree->Branch("npv",&_npv,"npv/I");  
  myTree->Branch("npu",&_npu,"npu/I"); 
  myTree->Branch("PUReweight",&_PUReweight,"PUReweight/F"); 
  myTree->Branch("rho",&_rho,"rho/F");  
  
  myTree->Branch("mothers_px",&_mothers_px);
  myTree->Branch("mothers_py",&_mothers_py);
  myTree->Branch("mothers_pz",&_mothers_pz);
  myTree->Branch("mothers_e",&_mothers_e);

  myTree->Branch("daughters_px",&_daughters_px);
  myTree->Branch("daughters_py",&_daughters_py);
  myTree->Branch("daughters_pz",&_daughters_pz);
  myTree->Branch("daughters_e",&_daughters_e);

  if(writeSoftLep)myTree->Branch("softLeptons",&_softLeptons);
  if(theisMC){
    myTree->Branch("genDaughters",&_genDaughters);
    myTree->Branch("quarks_px",&_bquarks_px);
    myTree->Branch("quarks_py",&_bquarks_py);
    myTree->Branch("quarks_pz",&_bquarks_pz);
    myTree->Branch("quarks_e",&_bquarks_e);
    myTree->Branch("quarks_pdg",&_bquarks_pdg);
    myTree->Branch("motmass",&_bmotmass);
    myTree->Branch("MC_weight",&_MC_weight,"MC_weight/F");
    myTree->Branch("genH_px",&_genH_px);
    myTree->Branch("genH_py",&_genH_py);
    myTree->Branch("genH_pz",&_genH_pz);
    myTree->Branch("genH_e",&_genH_e);
  }
  //myTree->Branch("daughters2",&_daughter2);
  myTree->Branch("SVfitMass",&_SVmass);
  myTree->Branch("isOSCand",&_isOSCand);
  myTree->Branch("METx",&_metx);
  myTree->Branch("METy",&_mety);
  myTree->Branch("MET_cov00",&_metCov00);
  myTree->Branch("MET_cov01",&_metCov01);
  myTree->Branch("MET_cov10",&_metCov10);
  myTree->Branch("MET_cov11",&_metCov11);  
  myTree->Branch("MET_significance",&_metSignif); 
  myTree->Branch("PDGIdDaughters",&_pdgdau);
  myTree->Branch("indexDau1",&_indexDau1);
  myTree->Branch("indexDau2",&_indexDau2);
  myTree->Branch("particleType",&_particleType);
  myTree->Branch("discriminator",&_discriminator);
  myTree->Branch("dxy",&_dxy);
  myTree->Branch("dz",&_dz);
  myTree->Branch("daughters_iseleBDT",&_daughters_iseleBDT);
  myTree->Branch("daughters_iseleCUT",&_daughters_iseleCUT);
  myTree->Branch("decayMode",&_decayType);
  myTree->Branch("combreliso",& _combreliso);
  myTree->Branch("daughters_IetaIeta",&_daughters_IetaIeta);
  myTree->Branch("daughters_deltaPhiSuperClusterTrackAtVtx",&_daughters_deltaPhiSuperClusterTrackAtVtx);
  myTree->Branch("daughters_depositR03_tracker",&_daughters_depositR03_tracker);
  myTree->Branch("daughters_depositR03_ecal",&_daughters_depositR03_ecal);
  myTree->Branch("daughters_depositR03_hcal",&_daughters_depositR03_hcal);
  myTree->Branch("daughters_decayModeFindingOldDMs", &_daughters_decayModeFindingOldDMs);
  myTree->Branch("daughters_decayModeFindingNewDMs", &_daughters_decayModeFindingNewDMs);
  myTree->Branch("daughters_byLooseCombinedIsolationDeltaBetaCorr3Hits", &_daughters_byLooseCombinedIsolationDeltaBetaCorr3Hits);
  myTree->Branch("daughters_byMediumCombinedIsolationDeltaBetaCorr3Hits", &_daughters_byMediumCombinedIsolationDeltaBetaCorr3Hits);
  myTree->Branch("daughters_byTightCombinedIsolationDeltaBetaCorr3Hits", &_daughters_byTightCombinedIsolationDeltaBetaCorr3Hits);
  myTree->Branch("daughters_byCombinedIsolationDeltaBetaCorrRaw3Hits", &_daughters_byCombinedIsolationDeltaBetaCorrRaw3Hits);
  myTree->Branch("daughters_chargedIsoPtSum", &_daughters_chargedIsoPtSum);
  myTree->Branch("daughters_neutralIsoPtSum", &_daughters_neutralIsoPtSum);
  myTree->Branch("daughters_puCorrPtSum", &_daughters_puCorrPtSum);
  myTree->Branch("daughters_againstMuonLoose3", &_daughters_againstMuonLoose3);
  myTree->Branch("daughters_againstMuonTight3", &_daughters_againstMuonTight3);
  myTree->Branch("daughters_againstElectronVLooseMVA5", &_daughters_againstElectronVLooseMVA5);
  myTree->Branch("daughters_againstElectronLooseMVA5", &_daughters_againstElectronLooseMVA5);
  myTree->Branch("daughters_againstElectronMediumMVA5", &_daughters_againstElectronMediumMVA5);
  myTree->Branch("daughters_isLastTriggerObjectforPath", &_daughters_LFtrigger);
  myTree->Branch("daughters_isTriggerObjectforPath", &_daughters_L3trigger);
  myTree->Branch("daughters_FilterFired",&_daughters_FilterFired);
  myTree->Branch("daughters_isGoodTriggerType",&_daughters_isGoodTriggerType);
  myTree->Branch("daughters_L3FilterFired",&_daughters_L3FilterFired);
  myTree->Branch("daughters_L3FilterFiredLast",&_daughters_L3FilterFiredLast);
  
  myTree->Branch("JetsNumber",&_numberOfJets,"JetsNumber/I");
  myTree->Branch("jets_px",&_jets_px);
  myTree->Branch("jets_py",&_jets_py);
  myTree->Branch("jets_pz",&_jets_pz);
  myTree->Branch("jets_e",&_jets_e);
  myTree->Branch("jets_Flavour",&_jets_Flavour);
  myTree->Branch("jets_PUJetID",&_jets_PUJetID);
  myTree->Branch("bDiscriminator",&_bdiscr);
  myTree->Branch("bCSVscore",&_bdiscr2);
}

Int_t HTauTauNtuplizer::FindCandIndex(const reco::Candidate& cand,Int_t iCand=0){
  const reco::Candidate *daughter = cand.daughter(iCand);
  for(UInt_t iLeptons=0;iLeptons<_softLeptons.size();iLeptons++){
    //if(daughter==daughterPoint[iLeptons]){
    //if(daughter==_softLeptons.at(iLeptons)){
    if(daughter->masterClone().get()==_softLeptons.at(iLeptons)){
      return iLeptons;
    }
  }
  return -1;
}
// ----Analyzer (main) ----
// ------------ method called for each event  ------------
void HTauTauNtuplizer::analyze(const edm::Event& event, const edm::EventSetup& eSetup)
{
  Initialize();

  Handle<vector<reco::Vertex> >  vertexs;
  event.getByLabel("goodPrimaryVertices",vertexs);
  
  //----------------------------------------------------------------------
  // Analyze MC history. THIS HAS TO BE DONE BEFORE ANY RETURN STATEMENT
  // (eg skim or trigger), in order to update the gen counters correctly!!!
  
  //  std::vector<const reco::Candidate *> genZs;
  // std::vector<const reco::Candidate *> genZLeps;
  // if (isMC) {
    
  Handle<std::vector< PileupSummaryInfo > >  PupInfo;
  event.getByLabel(edm::InputTag("addPileupInfo"), PupInfo);    
  std::vector<PileupSummaryInfo>::const_iterator PVI;
  for(PVI = PupInfo->begin(); PVI != PupInfo->end(); ++PVI) {
    if(PVI->getBunchCrossing() == 0) { 
      _npv = vertexs->size();
      _rho  = PVI->getPU_NumInteractions();
      int nTrueInt = PVI->getTrueNumInteractions();
      _npu = nTrueInt;
      _PUReweight = reweight.weight(2012,2012,nTrueInt);
   	  break;
    } 
  }
  // }
  
  triggerhelper myTriggerHelper;
  _triggerbit = myTriggerHelper.FindTriggerBit(event,foundPaths,indexOfPath);
  _metfilterbit = myTriggerHelper.FindMETBit(event);

  //Get candidate collection
  edm::Handle<edm::View<pat::CompositeCandidate>>candHandle;
  edm::Handle<edm::View<reco::Candidate>>dauHandle;
  edm::Handle<edm::View<pat::Jet>>jetHandle;
  edm::Handle<pat::METCollection> metHandle;
  edm::Handle<GenFilterInfo> embeddingWeightHandle;
  edm::Handle<edm::TriggerResults> triggerResults;
 
  // protect in case of events where trigger hasn't fired --> no collection created 
  event.getByLabel(theCandLabel,candHandle);
  if (!candHandle.isValid()) return;
  event.getByLabel(theCandLabel,candHandle);
  event.getByLabel("jets",jetHandle);
  event.getByLabel("softLeptons",dauHandle);
  event.getByLabel("slimmedMETs",metHandle);
  if(theisMC){
    event.getByLabel(edm::InputTag("generator","minVisPtFilter",""),embeddingWeightHandle);
    _MC_weight = (embeddingWeightHandle.isValid() ? embeddingWeightHandle->filterEfficiency():1.0);
  }

  const edm::View<pat::CompositeCandidate>* cands = candHandle.product();
  const edm::View<reco::Candidate>* daus = dauHandle.product();
  const edm::View<pat::Jet>* jets = jetHandle.product();
  const pat::MET &met = metHandle->front();
  //myNtuple->InitializeVariables();
    
  _indexevents = event.id().event();
  _runNumber = event.id().run();
  _lumi=event.luminosityBlock();
  _met = met.sumEt();
  _metphi = met.phi();
    
  //Do all the stuff here
  //Compute the variables needed for the output and store them in the ntuple
  if(DEBUG)printf("===New Event===\n");

  //Loop over generated b quarks
  if(theisMC)FillbQuarks(event);

  //Loop of softleptons and fill them
  FillSoftLeptons(daus,event,theFSR);

  //Loop on Jets
  _numberOfJets = 0;
  if(writeJets)_numberOfJets = FillJet(jets);

  //Loop on pairs
  std::vector<pat::CompositeCandidate> candVector;
  for(edm::View<pat::CompositeCandidate>::const_iterator candi = cands->begin(); candi!=cands->end();++candi){
    Npairs++;
    const pat::CompositeCandidate& cand = (*candi); 
    candVector.push_back(cand);
  }
  std::sort(candVector.begin(),candVector.end(),ComparePairs);
  for(int iPair=0;iPair<int(candVector.size());iPair++){
    const pat::CompositeCandidate& cand = candVector.at(iPair);
    math::XYZTLorentzVector candp4 = cand.p4();
    _SVmass.push_back(cand.userFloat("SVfitMass"));
    _metx.push_back(cand.userFloat("MEt_px"));
    _mety.push_back(cand.userFloat("MEt_py"));
    _metCov00.push_back(cand.userFloat("MEt_cov00"));
    _metCov01.push_back(cand.userFloat("MEt_cov01"));
    _metCov10.push_back(cand.userFloat("MEt_cov10"));
    _metCov11.push_back(cand.userFloat("MEt_cov11"));
    _metSignif.push_back(cand.userFloat("MEt_significance"));
    
    //if(DEBUG){
      //motherPoint[iMot]=dynamic_cast<const reco::Candidate*>(&*candi);
      //printf("%p %p %p\n",motherPoint[iMot],cand.daughter(0),cand.daughter(1));
    //}
    int index1=-1,index2=-1;
    for(int iCand=0;iCand<2;iCand++){
        //int index=-1;
        if(iCand==0) index1=FindCandIndex(cand,iCand);
        else index2=FindCandIndex(cand,iCand);
        const reco::Candidate *daughter = cand.daughter(iCand);
        if(theFSR){
            const pat::PFParticle* fsr=0;
            double maxPT=-1;
            const PhotonPtrVector* gammas = userdatahelpers::getUserPhotons(daughter);
            if (gammas!=0) {
                for (PhotonPtrVector::const_iterator g = gammas->begin();g!= gammas->end(); ++g) {
                    //const pat::Photon* gamma = g->get();
                    const pat::PFParticle* gamma = g->get();
                    double pt = gamma->pt();
                    if (pt>maxPT) {
                        maxPT = pt;
                        fsr = gamma;
                    }    
                }
            }
    
            //cand.addUserFloat("dauWithFSR",lepWithFsr); // Index of the cand daughter with associated FSR photon
    
            if (fsr!=0) {
              // Add daughter and set p4.
              candp4+=fsr->p4();
              //pfour+=fsr->p4();
              //      myCand.addDaughter(reco::ShallowCloneCandidate(fsr->masterClone()),"FSR"); //FIXME: fsr does not have a masterClone
              //pat::PFParticle myFsr(fsr);
              //myFsr.setPdgId(22); // Fix: photons that are isFromMu have abs(pdgId)=13!!!
              //cand.addDaughter(myFsr,"FSR");
              /*
              // Recompute iso for leptons with FSR    
              const Candidate* d = cand.daughter(iCand);
              float fsrCorr = 0; // The correction to PFPhotonIso
              if (!fsr->isFromMuon()) { // Type 1 photons should be subtracted from muon iso cones
                double dR = ROOT::Math::VectorUtil::DeltaR(fsr->momentum(),d->momentum());
                if (dR<0.4 && ((d->isMuon() && dR > 0.01) ||
                       (d->isElectron() && (fabs((static_cast<const pat::Electron*>(d->masterClone().get()))->superCluster()->eta()) < 1.479 || dR > 0.08)))) {
                  fsrCorr = fsr->pt();
                }
              }

              float rho = ((d->isMuon())?rhoForMu:rhoForEle);
              float combRelIsoPFCorr =  LeptonIsoHelper::combRelIsoPF(sampleType, setup, rho, d, fsrCorr);
      
              string base;
              stringstream str;
              str << "d" << iCand << ".";
              str >> base;      
              cand.addUserFloat(base+"combRelIsoPFFSRCorr",combRelIsoPFCorr);
              */
            }

            //daughter->setP4(daughter->p4()+fsr->p4());
        }

        //if(iCand==0)_indexDau1.push_back(index);
        //else _indexDau2.push_back(index);	
    }
    if(CompareLegs(cand.daughter(0),cand.daughter(1))){
      _indexDau1.push_back(index1);
      _indexDau2.push_back(index2);	
    }else {
      _indexDau1.push_back(index2);
      _indexDau2.push_back(index1);	    
    }
    if(fabs(cand.charge())>0.5)_isOSCand.push_back(false);
    else _isOSCand.push_back(true);
//    if(cand.charge()!=cand.daughter(0)->charge()+cand.daughter(1)->charge())cout<<"charge DIVERSA!!!!!!!!! "<<cand.charge()<<" "<<cand.daughter(0)->charge()<<" "<<cand.daughter(1)->charge()<<endl;
//    else cout<<"charge uguale "<<endl;
    _mothers_px.push_back( (float) candp4.X());
    _mothers_py.push_back( (float) candp4.Y());
    _mothers_pz.push_back( (float) candp4.Z());
    _mothers_e.push_back( (float) candp4.T());
    
    /*   float fillArray[nOutVars]={
    (float)event.id().run(),
    (float)event.id().event(),
    (float)cand.p4().mass(),
      (float)cand.p4().pt(),
      (float)cand.p4().eta(),
      (float)cand.p4().phi(),
      (float)cand.daughter(0)->mass(),
      (float)cand.daughter(0)->pt(),
      (float)cand.daughter(0)->eta(),
      (float)cand.daughter(0)->phi(),
      (float)cand.daughter(1)->mass(),
      (float)cand.daughter(1)->pt(),
      (float)cand.daughter(1)->eta(),
      (float)cand.daughter(1)->phi()
    };
    myTree->Fill(fillArray);*/
    //iMot++;
  }
  myTree->Fill();
  //return;
}

//Fill jets quantities
int HTauTauNtuplizer::FillJet(const edm::View<pat::Jet> *jets){
  int nJets=0;
  for(edm::View<pat::Jet>::const_iterator ijet = jets->begin(); ijet!=jets->end();++ijet){
    nJets++;
    _jets_px.push_back( (float) ijet->px());
    _jets_py.push_back( (float) ijet->py());
    _jets_pz.push_back( (float) ijet->pz());
    _jets_e.push_back( (float) ijet->energy());
    _jets_Flavour.push_back(ijet->partonFlavour());
    _jets_PUJetID.push_back(ijet->userFloat("pileupJetId:fullDiscriminant"));
    _bdiscr.push_back(ijet->bDiscriminator("jetBProbabilityBJetTags"));
    _bdiscr2.push_back(ijet->bDiscriminator("combinedInclusiveSecondaryVertexV2BJetTags"));
  }
  return nJets;
}

//Fill all leptons (we keep them all for veto purposes
void HTauTauNtuplizer::FillSoftLeptons(const edm::View<reco::Candidate> *daus, const edm::Event& event,bool theFSR){
  edm::Handle<edm::TriggerResults> triggerBits;
  edm::Handle<pat::TriggerObjectStandAloneCollection> triggerObjects;

  event.getByToken(triggerObjects_, triggerObjects);
  event.getByToken(triggerBits_, triggerBits);
  const edm::TriggerNames &names = event.triggerNames(*triggerBits);
  
  for(edm::View<reco::Candidate>::const_iterator daui = daus->begin(); daui!=daus->end();++daui){
    const reco::Candidate* cand = &(*daui);
    math::XYZTLorentzVector pfour = cand->p4();
    if(theFSR){
      const pat::PFParticle* fsr=0;
      double maxPT=-1;
      const PhotonPtrVector* gammas = userdatahelpers::getUserPhotons(cand);
      if (gammas!=0) {
        for (PhotonPtrVector::const_iterator g = gammas->begin();g!= gammas->end(); ++g) {
          //const pat::Photon* gamma = g->get();
          const pat::PFParticle* gamma = g->get();
          double pt = gamma->pt();
          if (pt>maxPT) {
            maxPT  = pt;
            fsr = gamma;
          }
        }
      }
      if (fsr!=0) {
        pfour+=fsr->p4();
      }
    } 
    
    _daughters_px.push_back( (float) pfour.X());
    _daughters_py.push_back( (float) pfour.Y());
    _daughters_pz.push_back( (float) pfour.Z());
    _daughters_e.push_back( (float) pfour.T());

    //math::XYZTLorentzVector pfour(userdatahelpers::getUserFloat(cand,"genPx"),userdatahelpers::getUserFloat(cand,"genPy"),userdatahelpers::getUserFloat(cand,"genPz"),userdatahelpers::getUserFloat(cand,"genE"));
    if(theisMC)_genDaughters.push_back(userdatahelpers::getUserFloat(cand,"fromH"));
    _softLeptons.push_back(cand);//This is needed also for FindCandIndex
    _pdgdau.push_back(cand->pdgId());
    _combreliso.push_back(userdatahelpers::getUserFloat(cand,"combRelIsoPF"));
    _dxy.push_back(userdatahelpers::getUserFloat(cand,"dxy"));
    _dz.push_back(userdatahelpers::getUserFloat(cand,"dz"));
    
    int type = ParticleType::TAU;
    if(cand->isMuon()) type = ParticleType::MUON;
    else if(cand->isElectron()) type = ParticleType::ELECTRON;
    _particleType.push_back(type);
    
    // variables
    float discr=-1;
    bool isgood = false,isgoodcut=false;
    int decay=-1;
    float ieta=-1,superatvtx=-1,depositTracker=-1,depositEcal=-1,depositHcal=-1;
    int decayModeFindingOldDMs=-1, decayModeFindingNewDMs=-1; // tau 13 TeV ID
    int byLooseCombinedIsolationDeltaBetaCorr3Hits=-1, byMediumCombinedIsolationDeltaBetaCorr3Hits=-1, byTightCombinedIsolationDeltaBetaCorr3Hits=-1; // tau 13 TeV Iso
    float byCombinedIsolationDeltaBetaCorrRaw3Hits=-1., chargedIsoPtSum=-1., neutralIsoPtSum=-1., puCorrPtSum=-1.; // tau 13 TeV RAW iso info
    int againstMuonLoose3=-1, againstMuonTight3=-1; // tau 13 TeV muon rejection
    int againstElectronVLooseMVA5 =-1, againstElectronLooseMVA5 = -1, againstElectronMediumMVA5 = -1; // tau 13 TeV ele rejection
    
    
    if(type==ParticleType::MUON){
      discr=userdatahelpers::getUserFloat(cand,"muonID");
      depositTracker=userdatahelpers::getUserFloat(cand,"DepositR03TrackerOfficial");
      depositEcal=userdatahelpers::getUserFloat(cand,"DepositR03Ecal");
      depositHcal=userdatahelpers::getUserFloat(cand,"DepositR03Hcal");
    }else if(type==ParticleType::ELECTRON){
      discr=userdatahelpers::getUserFloat(cand,"BDT");
      ieta=userdatahelpers::getUserFloat(cand,"sigmaIetaIeta");
      superatvtx=userdatahelpers::getUserFloat(cand,"deltaPhiSuperClusterTrackAtVtx");
      if(userdatahelpers::getUserInt(cand,"isBDT"))isgood=true;
      if(userdatahelpers::getUserInt(cand,"isCUT"))isgoodcut=true;
    }else if(type==ParticleType::TAU){
      discr=userdatahelpers::getUserFloat(cand,"HPSDiscriminator");
      decay = userdatahelpers::getUserFloat(cand,"decayMode");
      decayModeFindingOldDMs = userdatahelpers::getUserInt (cand, "decayModeFindingOldDMs");
      decayModeFindingNewDMs = userdatahelpers::getUserInt (cand, "decayModeFindingNewDMs");
      byLooseCombinedIsolationDeltaBetaCorr3Hits = userdatahelpers::getUserInt (cand, "byLooseCombinedIsolationDeltaBetaCorr3Hits");
      byMediumCombinedIsolationDeltaBetaCorr3Hits = userdatahelpers::getUserInt (cand, "byMediumCombinedIsolationDeltaBetaCorr3Hits");
      byTightCombinedIsolationDeltaBetaCorr3Hits = userdatahelpers::getUserInt (cand, "byTightCombinedIsolationDeltaBetaCorr3Hits");
      byCombinedIsolationDeltaBetaCorrRaw3Hits = userdatahelpers::getUserFloat (cand, "byCombinedIsolationDeltaBetaCorrRaw3Hits");
      chargedIsoPtSum = userdatahelpers::getUserFloat (cand, "chargedIsoPtSum");
      neutralIsoPtSum = userdatahelpers::getUserFloat (cand, "neutralIsoPtSum");
      puCorrPtSum = userdatahelpers::getUserFloat (cand, "puCorrPtSum");
      againstMuonLoose3 = userdatahelpers::getUserInt (cand, "againstMuonLoose3");
      againstMuonTight3 = userdatahelpers::getUserInt (cand, "againstMuonTight3");
      againstElectronVLooseMVA5 = userdatahelpers::getUserInt (cand, "againstElectronVLooseMVA5");
      againstElectronLooseMVA5 = userdatahelpers::getUserInt (cand, "againstElectronLooseMVA5");
      againstElectronMediumMVA5 = userdatahelpers::getUserInt (cand, "againstElectronMediumMVA5");
    }
    _discriminator.push_back(discr);
    _daughters_iseleBDT.push_back(isgood);
    _daughters_iseleCUT.push_back(isgoodcut);
    _decayType.push_back(decay);
    _daughters_IetaIeta.push_back(ieta);
    _daughters_deltaPhiSuperClusterTrackAtVtx.push_back(superatvtx);
    _daughters_depositR03_tracker.push_back(depositTracker);
    _daughters_depositR03_ecal.push_back(depositEcal);
    _daughters_depositR03_hcal.push_back(depositHcal);
    _daughters_decayModeFindingOldDMs.push_back(decayModeFindingOldDMs);
    _daughters_decayModeFindingNewDMs.push_back(decayModeFindingNewDMs);
    _daughters_byLooseCombinedIsolationDeltaBetaCorr3Hits.push_back(byLooseCombinedIsolationDeltaBetaCorr3Hits);
    _daughters_byMediumCombinedIsolationDeltaBetaCorr3Hits.push_back(byMediumCombinedIsolationDeltaBetaCorr3Hits);
    _daughters_byTightCombinedIsolationDeltaBetaCorr3Hits.push_back(byTightCombinedIsolationDeltaBetaCorr3Hits);
    _daughters_byCombinedIsolationDeltaBetaCorrRaw3Hits.push_back(byCombinedIsolationDeltaBetaCorrRaw3Hits);
    _daughters_chargedIsoPtSum.push_back(chargedIsoPtSum);
    _daughters_neutralIsoPtSum.push_back(neutralIsoPtSum);
    _daughters_puCorrPtSum.push_back(puCorrPtSum);
    _daughters_againstMuonLoose3.push_back(againstMuonLoose3);
    _daughters_againstMuonTight3.push_back(againstMuonTight3);
    _daughters_againstElectronVLooseMVA5.push_back(againstElectronVLooseMVA5);
    _daughters_againstElectronLooseMVA5.push_back(againstElectronLooseMVA5);
    _daughters_againstElectronMediumMVA5.push_back(againstElectronMediumMVA5);
    
    //TRIGGER MATCHING
    int LFtriggerbit=0,L3triggerbit=0,filterFired=0;
    bool triggerType=false;
    for (pat::TriggerObjectStandAlone obj : *triggerObjects) { 
      //check if the trigger object matches cand
      //bool matchCand = false;
      //if(type == ParticleType::TAU && 
      if(deltaR2(obj,*cand)<0.025){
        if (type==ParticleType::TAU && (obj.hasTriggerObjectType(trigger::TriggerTau)|| obj.hasTriggerObjectType(trigger::TriggerL1TauJet)))triggerType=true;
        if (type==ParticleType::ELECTRON && (obj.hasTriggerObjectType(trigger::TriggerElectron) || obj.hasTriggerObjectType(trigger::TriggerPhoton)))triggerType=true;
        if (type==ParticleType::MUON && (obj.hasTriggerObjectType(trigger::TriggerMuon)))triggerType=true;
        triggerhelper myTriggerHelper;
        //check fired paths
        obj.unpackPathNames(names);
        std::vector<std::string> pathNamesAll  = obj.pathNames(false);
        std::vector<std::string> pathNamesLast = obj.pathNames(true);
        for (unsigned h = 0, n = pathNamesAll.size(); h < n; ++h) {
          bool isLF   = obj.hasPathName( pathNamesAll[h], true, false ); 
          bool isL3   = obj.hasPathName( pathNamesAll[h], false, true );
          int triggerbit = myTriggerHelper.FindTriggerNumber(pathNamesAll[h],true);
          if(triggerbit>=0){
            triggerMapper map = myTriggerHelper.GetTriggerMap(pathNamesAll[h]);
            bool isfilterGood = true;
            if(type==ParticleType::TAU){
              for(int ifilt=0;ifilt<map.GetNfiltersleg2();ifilt++){
                if(! obj.hasFilterLabel(map.Getfilter(false,ifilt).Data()))isfilterGood=false;
              }
            }else if(type==ParticleType::ELECTRON){
              for(int ifilt=0;ifilt<map.GetNfiltersleg1();ifilt++){
                if(! obj.hasFilterLabel(map.Getfilter(true,ifilt).Data()))isfilterGood=false;
              }
            }else{//muons
              if(map.GetTriggerChannel()==triggerMapper::kemu){
                for(int ifilt=0;ifilt<map.GetNfiltersleg1();ifilt++){
                  if(! obj.hasFilterLabel(map.Getfilter(true,ifilt).Data()))isfilterGood=false;
                }
              }else{
                for(int ifilt=0;ifilt<map.GetNfiltersleg2();ifilt++){
                  if(! obj.hasFilterLabel(map.Getfilter(false,ifilt).Data()))isfilterGood=false;
                }
              }
            }
          //_isFilterFiredLast;
            if(isfilterGood)filterFired |= 1 <<triggerbit;
            if(isLF)LFtriggerbit |= 1 <<triggerbit;
            if(isL3)L3triggerbit |= 1 <<triggerbit;
          }
        }
      }
    }
    _daughters_isGoodTriggerType.push_back(triggerType);
    _daughters_FilterFired.push_back(filterFired);
    _daughters_L3FilterFired.push_back(LFtriggerbit);
    _daughters_L3FilterFiredLast.push_back(L3triggerbit);    
  }
}

void HTauTauNtuplizer::FillbQuarks(const edm::Event& event){
  edm::Handle<edm::View<pat::GenericParticle>>candHandle;
  event.getByLabel("bQuarks",candHandle);
  const edm::View<pat::GenericParticle>* bs = candHandle.product();
  for(edm::View<pat::GenericParticle>::const_iterator ib = bs->begin(); ib!=bs->end();++ib){
    const pat::GenericParticle* cand = &(*ib);
    _bquarks_px.push_back( (float) cand->px());
    _bquarks_py.push_back( (float) cand->py());
    _bquarks_pz.push_back( (float) cand->px());
    _bquarks_e.push_back( (float) cand->energy());
    _bquarks_pdg.push_back( (int) cand->pdgId());
    _bmotmass.push_back(userdatahelpers::getUserFloat(cand,"motHmass"));
  }

  //Retrieve Generated H (there can be more than 1!  
  Handle<edm::View<reco::GenParticle> > prunedHandle;
  event.getByLabel("prunedGenParticles", prunedHandle);
  for(unsigned int ipruned = 0; ipruned< prunedHandle->size(); ++ipruned){
     const GenParticle *packed =&(*prunedHandle)[ipruned];
     int pdgh = packed->pdgId();
     if(abs(pdgh)==25){
        // avoid Higgs clones, save only the one not going into another H (end of showering process)
        bool isLast = true;
        for (unsigned int iDau = 0; iDau < packed->numberOfDaughters(); iDau++)
        {
            const Candidate* Dau = packed->daughter( iDau );
            if (Dau->pdgId() == pdgh)
            {
                isLast = false;
                break;
            }
        }
   
        if (isLast)
        {     
            _genH_px.push_back(packed->px());          
            _genH_py.push_back(packed->py());          
            _genH_pz.push_back(packed->pz());          
            _genH_e.push_back(packed->energy());          
        }
     }        
  } 
}

void HTauTauNtuplizer::endJob(){
  hCounter->SetBinContent(1,Nevt_Gen);
  hCounter->SetBinContent(2,Nevt_PassTrigger);
  hCounter->SetBinContent(3,Npairs);

  hCounter->GetXaxis()->SetBinLabel(1,"Nevt_Gen");
  hCounter->GetXaxis()->SetBinLabel(2,"Nevt_PassTrigger");
  hCounter->GetXaxis()->SetBinLabel(3,"Npairs");
}


void HTauTauNtuplizer::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup){
  
  Bool_t changedConfig = false;
 
  //if(!hltConfig_.init(iRun, iSetup, triggerResultsLabel.process(), changedConfig)){
  if(!hltConfig_.init(iRun, iSetup, processName.process(), changedConfig)){
    edm::LogError("HLTMatchingFilter") << "Initialization of HLTConfigProvider failed!!"; 
    return;
  }  

  if(changedConfig || foundPaths.size()==0){
    //cout<<"The present menu is "<<hltConfig.tableName()<<endl;
    indexOfPath.clear();
    foundPaths.clear();
    //for(size_t i=0; i<triggerPaths.size(); i++){
    // bool foundThisPath = false;
    for(size_t j=0; j<hltConfig_.triggerNames().size(); j++){
      string pathName = hltConfig_.triggerNames()[j];
      //TString tempo= hltConfig_.triggerNames()[j];
      //printf("%s\n",tempo.Data());
      //if(pathName==triggerPaths[i]){
      //foundThisPath = true;
      indexOfPath.push_back(j);
      foundPaths.push_back(pathName);
	  //	  edm::LogInfo("AnalyzeRates")<<"Added path "<<pathName<<" to foundPaths";
    } 
  }
  
}


void HTauTauNtuplizer::endRun(edm::Run const&, edm::EventSetup const&){
}
void HTauTauNtuplizer::beginLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&){
}
void HTauTauNtuplizer::endLuminosityBlock(edm::LuminosityBlock const& iLumi, edm::EventSetup const& iSetup)
{
  // Total number of events is the sum of the events in each of these luminosity blocks
  edm::Handle<edm::MergeableCounter> nEventsTotalCounter;
  iLumi.getByLabel("nEventsTotal", nEventsTotalCounter);
  Nevt_Gen += nEventsTotalCounter->value;

  edm::Handle<edm::MergeableCounter> nEventsPassTrigCounter;
  iLumi.getByLabel("nEventsPassTrigger", nEventsPassTrigCounter);
  Nevt_PassTrigger += nEventsPassTrigCounter->value;
}


bool HTauTauNtuplizer::CompareLegs(const reco::Candidate *i, const reco::Candidate *j){
  int iType=2,jType=2;
  
  if(i->isElectron())iType=0;
  else if(i->isMuon())iType=1;
  
  if(j->isElectron())jType=0;
  else if(j->isMuon())jType=1;
  
  if(iType>jType) return false;
  else if(iType==jType && i->pt()<j->pt()) return false;
  
  return true;
}

// // ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
// void HTauTauNtuplizer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
//   //The following says we do not know what parameters are allowed so do no validation
//   // Please change this to state exactly what you do use, even if it is no parameters
//   edm::ParameterSetDescription desc;
//   desc.setUnknown();
//   descriptions.addDefault(desc);
// }

//define this as a plug-in
DEFINE_FWK_MODULE(HTauTauNtuplizer);
