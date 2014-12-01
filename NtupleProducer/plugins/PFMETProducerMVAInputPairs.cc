#include "LLRHiggsTauTau/NtupleProducer/plugins/PFMETProducerMVAInputPairs.h"

#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "RecoMET/METAlgorithms/interface/METAlgo.h" 
#include "RecoMET/METAlgorithms/interface/PFSpecificAlgo.h"
#include "DataFormats/Candidate/interface/CandidateFwd.h"
#include "DataFormats/PatCandidates/interface/Tau.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/TauReco/interface/PFTau.h"
#include "DataFormats/MuonReco/interface/Muon.h"
#include "DataFormats/EgammaCandidates/interface/GsfElectron.h"
#include "DataFormats/METReco/interface/PFMET.h"
#include "DataFormats/METReco/interface/PFMETCollection.h"
#include "DataFormats/GsfTrackReco/interface/GsfTrack.h"
#include "DataFormats/GsfTrackReco/interface/GsfTrackFwd.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/Candidate/interface/Candidate.h"
#include "DataFormats/Common/interface/View.h"
#include "DataFormats/METReco/interface/CommonMETData.h"
#include "DataFormats/Candidate/interface/ShallowCloneCandidate.h"
#include "DataFormats/PatCandidates/interface/CompositeCandidate.h"
#include "JetMETCorrections/Algorithms/interface/L1FastjetCorrector.h"
#include "TLorentzVector.h"


#include "DataFormats/Math/interface/LorentzVector.h"

#include <algorithm>

using namespace reco;
//using namespace edm;

const double dR2Min = 0.01*0.01;
const double dR2Max = 0.5*0.5;
const double dPtMatch = 0.1;

namespace
{
  template <typename T>
  std::string format_vT(const std::vector<T>& vT)
  {
    std::ostringstream os;
  
    os << "{ ";

    unsigned numEntries = vT.size();
    for ( unsigned iEntry = 0; iEntry < numEntries; ++iEntry ) {
      os << vT[iEntry];
      if ( iEntry < (numEntries - 1) ) os << ", ";
    }

    os << " }";
  
    return os.str();
  }

  std::string format_vInputTag(const std::vector<edm::InputTag>& vit)
  {
    std::vector<std::string> vit_string;
    for ( std::vector<edm::InputTag>::const_iterator vit_i = vit.begin();
	  vit_i != vit.end(); ++vit_i ) {
      vit_string.push_back(vit_i->label());
    }
    return format_vT(vit_string);
  }

  void printJets(std::ostream& stream, const reco::PFJetCollection& jets)
  {
    unsigned numJets = jets.size();
    for ( unsigned iJet = 0; iJet < numJets; ++iJet ) {
      const reco::Candidate::LorentzVector& jetP4 = jets.at(iJet).p4();
      stream << " #" << iJet << ": Pt = " << jetP4.pt() << "," 
	     << " eta = " << jetP4.eta() << ", phi = " << jetP4.phi() << std::endl;
    }
  }
}

PFMETProducerMVAInputPairs::PFMETProducerMVAInputPairs(const edm::ParameterSet& cfg) 
  : mvaMEtAlgo_(cfg),
    mvaMEtAlgo_isInitialized_(false),
    mvaJetIdAlgo_(cfg)   
{
  srcCorrJets_     = consumes<reco::PFJetCollection>(cfg.getParameter<edm::InputTag>("srcCorrJets"));
  srcUncorrJets_   = consumes<reco::PFJetCollection>(cfg.getParameter<edm::InputTag>("srcUncorrJets"));
  srcPFCandidates_ = consumes<reco::PFCandidateCollection>(cfg.getParameter<edm::InputTag>("srcPFCandidates"));
  srcPFCandidatesView_ = consumes<edm::View<reco::Candidate> >(cfg.getParameter<edm::InputTag>("srcPFCandidates"));
  srcVertices_     = consumes<reco::VertexCollection>(cfg.getParameter<edm::InputTag>("srcVertices"));
  srcLeptons_      = consumes<pat::CompositeCandidate>(cfg.getParameter<edm::InputTag>("srcPairs")); // srcLeptons_ contains lepton pairs

  minNumLeptons_   = cfg.getParameter<int>("minNumLeptons");
  srcRho_          = consumes<edm::Handle<double> >(cfg.getParameter<edm::InputTag>("srcRho"));

  globalThreshold_ = cfg.getParameter<double>("globalThreshold");

  minCorrJetPt_    = cfg.getParameter<double>     ("minCorrJetPt");
  useType1_        = cfg.getParameter<bool>       ("useType1");
  correctorLabel_  = cfg.getParameter<std::string>("corrector");
  isOld42_         = cfg.getParameter<bool>       ("useOld42");
  
  verbosity_ = ( cfg.exists("verbosity") ) ?
    cfg.getParameter<int>("verbosity") : 0;

  //if ( verbosity_ ) {
  LogDebug( "PFMETProducerMVA" )
    << "<PFMETProducerMVA::PFMETProducerMVA>:" << std::endl;
    // std::cout << " srcCorrJets = " << srcCorrJets_.label() << std::endl;
    // std::cout << " srcUncorrJets = " << srcUncorrJets_.label() << std::endl;
    // std::cout << " srcPFCandidates = " << srcPFCandidates_.label() << std::endl;
    // std::cout << " srcVertices = " << srcVertices_.label() << std::endl;
    // std::cout << " srcLeptons = " << format_vInputTag(srcLeptons_) << std::endl;
    // std::cout << " srcRho = " << srcVertices_.label() << std::endl;

  produces<reco::PFMETCollection>();
}

PFMETProducerMVAInputPairs::~PFMETProducerMVAInputPairs(){}

void PFMETProducerMVAInputPairs::produce(edm::Event& evt, const edm::EventSetup& es) 
{ 
  // guard removed as pairs will always have two elements
  // CV: check if the event is to be skipped
  /*
  if ( minNumLeptons_ > 0 ) {
    int numLeptons = 0;
    for ( std::vector<edm::EDGetTokenT<edm::View<reco::Candidate> > >::const_iterator srcLeptons_i = srcLeptons_.begin();
	  srcLeptons_i != srcLeptons_.end(); ++srcLeptons_i ) {
      edm::Handle<CandidateView> leptons;
      evt.getByToken(*srcLeptons_i, leptons);
      numLeptons += leptons->size();
    }
    if ( !(numLeptons >= minNumLeptons_) ) {
      LogDebug( "produce" )
	<< "<PFMETProducerMVA::produce>:" << std::endl
	<< "Run: " << evt.id().run() << ", LS: " << evt.luminosityBlock()  << ", Event: " << evt.id().event() << std::endl
	<< " numLeptons = " << numLeptons << ", minNumLeptons = " << minNumLeptons_ << " --> skipping !!" << std::endl;
      
      reco::PFMET pfMEt;
      std::auto_ptr<reco::PFMETCollection> pfMEtCollection(new reco::PFMETCollection());
      pfMEtCollection->push_back(pfMEt);
      evt.put(pfMEtCollection);
      return;
    }
  }
  */
  
  // get jets (corrected and uncorrected)
  edm::Handle<reco::PFJetCollection> corrJets;
  evt.getByToken(srcCorrJets_, corrJets);

  edm::Handle<reco::PFJetCollection> uncorrJets;
  evt.getByToken(srcUncorrJets_, uncorrJets);

  const JetCorrector* corrector = nullptr;
  if( useType1_ ) corrector = JetCorrector::getJetCorrector(correctorLabel_, es);

  // get PFCandidates
  edm::Handle<reco::PFCandidateCollection> pfCandidates;
  evt.getByToken(srcPFCandidates_, pfCandidates);

  edm::Handle<CandidateView> pfCandidates_view;
  evt.getByToken(srcPFCandidatesView_, pfCandidates_view);

  // get vertices
  edm::Handle<reco::VertexCollection> vertices;
  evt.getByToken(srcVertices_, vertices); 
  // take vertex with highest sum(trackPt) as the vertex of the "hard scatter" interaction
  // (= first entry in vertex collection)
  const reco::Vertex* hardScatterVertex = ( vertices->size() >= 1 ) ?
    &(vertices->front()) : 0;

  // get lepton pairs
  edm::Handle<edm::View<reco::CompositeCandidate> > pairHandle;
  evt.getByToken(srcLeptons_, pairHandle);
  
  // output collection
  std::auto_ptr<reco::PFMETCollection> pfMEtCollection(new reco::PFMETCollection());
  
  // initialize MVA MET algorithm
  // (this will load the BDTs, stored as GBRForrest objects;
  //  either in input ROOT files or in SQL-lite files/the Conditions Database) 
  if ( !mvaMEtAlgo_isInitialized_ ) {
     mvaMEtAlgo_.initialize(es);
     mvaMEtAlgo_isInitialized_ = true;
  }

  // reconstruct "standard" particle-flow missing Et
  // MM cahnged needed in 72X, need to check resutls are consistent with 53X
  //53X
  //CommonMETData pfMEt_data = metAlgo_.run(pfCandidates_view, globalThreshold_);
  //reco::PFMET pfMEt = pfMEtSpecificAlgo_.addInfo(pfCandidates_view, pfMEt_data);
  
  //72X
  CommonMETData pfMEt_data = metAlgo_.run( (*pfCandidates_view), globalThreshold_);
  SpecificPFMETData specificPfMET = pfMEtSpecificAlgo_.run( (*pfCandidates_view) );
  const reco::Candidate::LorentzVector p4( pfMEt_data.mex, pfMEt_data.mey, 0.0, pfMEt_data.met);
  const reco::Candidate::Point vtx(0.0, 0.0, 0.0 );
  reco::PFMET pfMEt(specificPfMET,pfMEt_data.sumet, p4, vtx);
  reco::Candidate::LorentzVector pfMEtP4_original = pfMEt.p4();

  // compute objects specific to MVA based MET reconstruction
  std::vector<mvaMEtUtilities::pfCandInfo> pfCandidateInfo = computePFCandidateInfo(*pfCandidates, hardScatterVertex);
  std::vector<reco::Vertex::Point>         vertexInfo      = computeVertexInfo(*vertices);

// ISSUE 1:
//  
// !!! ATTENZIONE! PROBABILMENTE DA FISSARE; LEPTONINFO CONTIENE SOLO LE INFORMAZIONI SUL LEPTONE 1, MA CI SI LOOPPA SOPRA SU TUTTO IL RANGE DI INPUT,
// PER CUI ALLA FINE CONTIENE TUTTI I LEPTONI NECESSARI. CREDO CHE CONTENGA I DUE (O PIÙ???) LEPTONI SU CUI EFFETTUARE LA SOMMA DELLA MET,
// IN PRATICA I DUE LEPTONI DELLA COPPIA DA TRATTARE IN MODO SPECIALE CON IL EMTODO MVA
//
// QUINDI PROBABILMENTE IN QUESO CASO NEL VETTORE LEPTONINFO DEVO METTERE DIRETTAMENTE I DUE FIGLI DELLA COPPIA  
//
// ISSUE 2:
//
// LA MET E SALVATA FUORI DAL LOOP, PERCHÉ?? COME MAI NON PRODUCE UN VETTORE DI MET? --> DA CONTROLLARE
//
// MODIFICA: la MET ora è calcolata per coppie e si salva una MET per coppia (prima era una unica MET MVA per evento)

  // loop on all the pairs
  for (unsigned int i = 0; i < pairHandle->size(); ++i)
  { 
     // create objects to be used as input in the MVA MET calculator
     std::vector<mvaMEtUtilities::leptonInfo> leptonInfo;
     bool lHasPhotons = false;
     
     // get leptons (the input pair)
     // (excluded from sum over PFCandidates when computing hadronic recoil)
     const CompositeCandidate& pair = (*pairHandle)[i];
    
     const Candidate *l1 = pair.daughter(0);
     const Candidate *l2 = pair.daughter(1);
  
    		// loop sulle differenti collezioni in input
			//  for ( std::vector<edm::EDGetTokenT<edm::View<reco::Candidate> > >::const_iterator srcLeptons_i = srcLeptons_.begin();
			//	srcLeptons_i != srcLeptons_.end(); ++srcLeptons_i ) {
  
   			 //edm::Handle<CandidateView> leptons;
    			//evt.getByToken(*srcLeptons_i, leptons);
  
 				 // loop sugli elementi della 1 collezione
  				 // for ( CandidateView::const_iterator lepton1 = leptons->begin();
				//  lepton1 != leptons->end(); ++lepton1 ) {
  
  	 // WARNING: all the original checks on the two input leptons are commented here
  	 // because they result in a "break" instruction that is applied inside the loop on the candidates of one lepton collection
  	 // but here we simply have a pair collection (and two leptons for each pair, i.e. no loop!)
  	 // Still need to understand if they are necessary and eventually fix them inserting a "continue" instruction
     
     /*
      bool pMatch = false; // TO FIX: PROBABLY THESE CHECKS CAN BE AVOIDED THANKS TO THE EXTERNAL PAIR COMBINER
 
 				// loop sulle differenti collezioni in input
     			// for ( std::vector<edm::EDGetTokenT<edm::View<reco::Candidate> > >::const_iterator srcLeptons_j = srcLeptons_.begin();
				 //   srcLeptons_j != srcLeptons_.end(); ++srcLeptons_j ) {

				//edm::Handle<CandidateView> leptons2;
				//evt.getByToken(*srcLeptons_j, leptons2);

				// loop sugli elementi della 2 colelzione
				//for ( CandidateView::const_iterator lepton2 = leptons2->begin();
				//      lepton2 != leptons2->end(); ++lepton2 ) {

	  if(&(*l1) == &(*l2)) { continue; } // kept as before, probably unuseful with CloneCombiner
	  if(deltaR2(l1->p4(),l2->p4()) < dR2Max) {pMatch = true;}              // guard against particles that are close in DeltaR
	  if(pMatch &&   !istau(&(*l1))   &&  istau(&(*l2))) {pMatch = false;}  // but keep if they are different particles
	  if(pMatch &&   ( (istau(&(*l1)) && istau(&(*l2))) || (!istau(&(*l1)) && !istau(&(*l2))))  && l1->pt() > l2->pt()) { pMatch = false; } // and keep if same particle, collinear but pt1 > pt2
	
	  if(pMatch && l1->pt() == l2->pt()) { // this check controls if in any previous saved lepton has a very close pt (why?)
	    pMatch = false;
	    for(unsigned int i0 = 0; i0 < leptonInfo.size(); i0++) {
	      if(std::abs(l1->pt() - leptonInfo[i0].p4_.pt()) < dPtMatch) { pMatch = true; break; }
	    }
	  }
	  
	  if(pMatch) break;
	}
	if(pMatch) break;
      }
      if(pMatch) continue;
     */
     
    mvaMEtUtilities::leptonInfo pLeptonInfo1;
    pLeptonInfo1.p4_          = l1->p4();
    pLeptonInfo1.chargedFrac_ = chargedFrac(&(*l1),*pfCandidates,hardScatterVertex);
    leptonInfo.push_back(pLeptonInfo1);
    
    mvaMEtUtilities::leptonInfo pLeptonInfo2;
    pLeptonInfo2.p4_          = l2->p4();
    pLeptonInfo2.chargedFrac_ = chargedFrac(&(*l2),*pfCandidates,hardScatterVertex);
    leptonInfo.push_back(pLeptonInfo2);
    
    if(l1->isPhoton() || l2->isPhoton()) { lHasPhotons = true; }
    //}
    
    //lId++; // WARNING : NOT SURE LID SHOULD BE INCREMENTED HERE!! ==> but is never used yeah!

  
  //if(lNMu == 2) std::cout << "=====> Di Muon Cand =======>"  << leptonInfo[0].p4_.pt() << " -- " << leptonInfo[1].p4_.pt() << std::endl;

    // get average energy density in the event
    //edm::Handle<double> rho;
    //evt.getByToken(srcRho_, rho);
    
    std::vector<mvaMEtUtilities::JetInfo>    jetInfo         = computeJetInfo(*uncorrJets, *corrJets, *vertices, hardScatterVertex, *corrector,evt,es,leptonInfo,pfCandidateInfo);
     
    // compute MVA based MET and estimate of its uncertainty
    mvaMEtAlgo_.setInput(leptonInfo, jetInfo, pfCandidateInfo, vertexInfo);
    mvaMEtAlgo_.setHasPhotons(lHasPhotons);
    mvaMEtAlgo_.evaluateMVA();
    pfMEt.setP4(mvaMEtAlgo_.getMEt());
    pfMEt.setSignificanceMatrix(mvaMEtAlgo_.getMEtCov());
  
    LogDebug("produce")
      << "<PFMETProducerMVA::produce>:" << std::endl
      << "Run: " << evt.id().run() << ", LS: " << evt.luminosityBlock()  << ", Event: " << evt.id().event() << std::endl
      << " PFMET: Pt = " << pfMEtP4_original.pt() << ", phi = " << pfMEtP4_original.phi() << " "
      << "(Px = " << pfMEtP4_original.px() << ", Py = " << pfMEtP4_original.py() << ")" << std::endl
      << " MVA MET: Pt = " << pfMEt.pt() << " phi = " << pfMEt.phi() << " (Px = " << pfMEt.px() << ", Py = " << pfMEt.py() << ")" << std::endl
      << " Cov:" << std::endl
      <<(mvaMEtAlgo_.getMEtCov())(0,0)<<"  "<<(mvaMEtAlgo_.getMEtCov())(0,1)<<std::endl
      <<(mvaMEtAlgo_.getMEtCov())(1,0)<<"  "<<(mvaMEtAlgo_.getMEtCov())(1,1)<<std::endl  << std::endl;
    //mvaMEtAlgo_.print(std::cout);
  
    // add PFMET object to the event
    pfMEtCollection->push_back(pfMEt);
  }

  std::cout << "DEBUG: Input contains " << pairHandle->size() << " pairs" << std::endl;
  std::cout << "DEBUG: Output contains " << pfMEtCollection->size() << " MET values" << std::endl; 
  
  evt.put(pfMEtCollection);
}

std::vector<mvaMEtUtilities::JetInfo> PFMETProducerMVAInputPairs::computeJetInfo(const reco::PFJetCollection& uncorrJets, 
								       const reco::PFJetCollection& corrJets, 
								       const reco::VertexCollection& vertices,
								       const reco::Vertex* hardScatterVertex,
								       const JetCorrector &iCorrector,edm::Event &iEvent,const edm::EventSetup &iSetup,
								       std::vector<mvaMEtUtilities::leptonInfo> &iLeptons,std::vector<mvaMEtUtilities::pfCandInfo> &iCands)
{

  const L1FastjetCorrector* lCorrector = dynamic_cast<const L1FastjetCorrector*>(&iCorrector);
  std::vector<mvaMEtUtilities::JetInfo> retVal;
  for ( reco::PFJetCollection::const_iterator uncorrJet = uncorrJets.begin();
	uncorrJet != uncorrJets.end(); ++uncorrJet ) {
    //int pIndex = uncorrJet-uncorrJets.begin();
    //edm::RefToBase<reco::Jet> jetRef(edm::Ref<PFJetCollection>(&uncorrJets,pIndex));
    for ( reco::PFJetCollection::const_iterator corrJet = corrJets.begin();
	  corrJet != corrJets.end(); ++corrJet ) {
      // match corrected and uncorrected jets
      if ( uncorrJet->jetArea() != corrJet->jetArea() ) continue;
      if ( deltaR2(corrJet->p4(),uncorrJet->p4()) > dR2Min ) continue;

      // check that jet passes loose PFJet id.
      //bool passesLooseJetId = (*looseJetIdAlgo_)(*corrJet);
      //if ( !passesLooseJetId ) continue; 
      if(!passPFLooseId(&(*uncorrJet))) continue;

      // compute jet energy correction factor
      // (= ratio of corrected/uncorrected jet Pt)
      double jetEnCorrFactor = corrJet->pt()/uncorrJet->pt();
      mvaMEtUtilities::JetInfo jetInfo;
      
      // PH: apply jet energy corrections for all Jets ignoring recommendations
      jetInfo.p4_ = corrJet->p4();
      double lType1Corr = 0;
      if(useType1_) { //Compute the type 1 correction ===> This code is crap 
	double pCorr = lCorrector->correction(*uncorrJet,iEvent,iSetup); //Does not work in 42X 
	lType1Corr = (corrJet->pt()-pCorr*uncorrJet->pt());
	TLorentzVector pVec; pVec.SetPtEtaPhiM(lType1Corr,0,corrJet->phi(),0); 
	reco::Candidate::LorentzVector pType1Corr; pType1Corr.SetCoordinates(pVec.Px(),pVec.Py(),pVec.Pz(),pVec.E());
	//Filter to leptons
	bool pOnLepton = false;
	for(unsigned int i0 = 0; i0 < iLeptons.size(); i0++) if(deltaR2(iLeptons[i0].p4_,corrJet->p4()) < dR2Max) {pOnLepton = true; break;}
	//Add it to PF Collection
	if(corrJet->pt() > 10 && !pOnLepton) {
	  mvaMEtUtilities::pfCandInfo pfCandidateInfo;
	  pfCandidateInfo.p4_ = pType1Corr;
	  pfCandidateInfo.dZ_ = -999;
	  iCands.push_back(pfCandidateInfo);
	}
	//Scale
	lType1Corr = (pCorr*uncorrJet->pt()-uncorrJet->pt());
	lType1Corr /=corrJet->pt();
      }
      
      // check that jet Pt used to compute MVA based jet id. is above threshold
      if ( !(jetInfo.p4_.pt() > minCorrJetPt_) ) continue;
      jetInfo.mva_ = mvaJetIdAlgo_.computeIdVariables(&(*corrJet), jetEnCorrFactor, hardScatterVertex, vertices, true).mva();
      jetInfo.neutralEnFrac_ = (uncorrJet->neutralEmEnergy() + uncorrJet->neutralHadronEnergy())/uncorrJet->energy();
      if(useType1_) jetInfo.neutralEnFrac_ -= lType1Corr*jetInfo.neutralEnFrac_;
      retVal.push_back(jetInfo);
      break;
    }
  }
  return retVal;
}

std::vector<mvaMEtUtilities::pfCandInfo> PFMETProducerMVAInputPairs::computePFCandidateInfo(const reco::PFCandidateCollection& pfCandidates,
										  const reco::Vertex* hardScatterVertex)
{
  std::vector<mvaMEtUtilities::pfCandInfo> retVal;
  for ( reco::PFCandidateCollection::const_iterator pfCandidate = pfCandidates.begin();
	pfCandidate != pfCandidates.end(); ++pfCandidate ) {
    double dZ = -999.; // PH: If no vertex is reconstructed in the event
                       //     or PFCandidate has no track, set dZ to -999
    if ( hardScatterVertex ) {
      if      ( pfCandidate->trackRef().isNonnull()    ) dZ = std::abs(pfCandidate->trackRef()->dz(hardScatterVertex->position()));
      else if ( pfCandidate->gsfTrackRef().isNonnull() ) dZ = std::abs(pfCandidate->gsfTrackRef()->dz(hardScatterVertex->position()));
    }
    mvaMEtUtilities::pfCandInfo pfCandidateInfo;
    pfCandidateInfo.p4_ = pfCandidate->p4();
    pfCandidateInfo.dZ_ = dZ;
    retVal.push_back(pfCandidateInfo);
  }
  return retVal;
}

std::vector<reco::Vertex::Point> PFMETProducerMVAInputPairs::computeVertexInfo(const reco::VertexCollection& vertices)
{
  std::vector<reco::Vertex::Point> retVal;
  for ( reco::VertexCollection::const_iterator vertex = vertices.begin();
	vertex != vertices.end(); ++vertex ) {
    if(std::abs(vertex->z())           > 24.) continue;
    if(vertex->ndof()              <  4.) continue;
    if(vertex->position().Rho()    >  2.) continue;
    retVal.push_back(vertex->position());
  }
  return retVal;
}
double PFMETProducerMVAInputPairs::chargedFrac(const reco::Candidate *iCand,
				     const reco::PFCandidateCollection& pfCandidates,const reco::Vertex* hardScatterVertex) { 
  if(iCand->isMuon())     {
    return 1;
  }
  if(iCand->isElectron())   {
    return 1.;
  }
  if(iCand->isPhoton()  )   {return chargedFracInCone(iCand, pfCandidates,hardScatterVertex);}
  double lPtTot = 0; double lPtCharged = 0;
  const reco::PFTau *lPFTau = 0; 
  lPFTau = dynamic_cast<const reco::PFTau*>(iCand);//} 
  if(lPFTau != 0) { 
    for (UInt_t i0 = 0; i0 < lPFTau->signalPFCands().size(); i0++) { 
      lPtTot += (lPFTau->signalPFCands())[i0]->pt(); 
      if((lPFTau->signalPFCands())[i0]->charge() == 0) continue;
      lPtCharged += (lPFTau->signalPFCands())[i0]->pt(); 
    }
  } 
  else { 
    const pat::Tau *lPatPFTau = nullptr; 
    lPatPFTau = dynamic_cast<const pat::Tau*>(iCand);//} 
    if(lPatPFTau != nullptr) { 
      for (UInt_t i0 = 0; i0 < lPatPFTau->signalPFCands().size(); i0++) { 
	lPtTot += (lPatPFTau->signalPFCands())[i0]->pt(); 
	if((lPatPFTau->signalPFCands())[i0]->charge() == 0) continue;
	lPtCharged += (lPatPFTau->signalPFCands())[i0]->pt(); 
      }
    }
  }
  if(lPtTot == 0) lPtTot = 1.;
  return lPtCharged/lPtTot;
}
//Return tau id by process of elimination
bool PFMETProducerMVAInputPairs::istau(const reco::Candidate *iCand) { 
  if(iCand->isMuon())     return false;
  if(iCand->isElectron()) return false;
  if(iCand->isPhoton())   return false;
  return true;
}
bool PFMETProducerMVAInputPairs::passPFLooseId(const PFJet *iJet) { 
  if(iJet->energy()== 0)                                  return false;
  if(iJet->neutralHadronEnergy()/iJet->energy() > 0.99)   return false;
  if(iJet->neutralEmEnergy()/iJet->energy()     > 0.99)   return false;
  if(iJet->nConstituents() <  2)                          return false;
  if(iJet->chargedHadronEnergy()/iJet->energy() <= 0 && std::abs(iJet->eta()) < 2.4 ) return false;
  if(iJet->chargedEmEnergy()/iJet->energy() >  0.99  && std::abs(iJet->eta()) < 2.4 ) return false;
  if(iJet->chargedMultiplicity()            < 1      && std::abs(iJet->eta()) < 2.4 ) return false;
  return true;
}

double PFMETProducerMVAInputPairs::chargedFracInCone(const reco::Candidate *iCand,
					   const reco::PFCandidateCollection& pfCandidates,
					   const reco::Vertex* hardScatterVertex,double iDRMax)
{

  double iDR2Max = iDRMax*iDRMax;
  reco::Candidate::LorentzVector lVis(0,0,0,0);
  for ( reco::PFCandidateCollection::const_iterator pfCandidate = pfCandidates.begin();
	pfCandidate != pfCandidates.end(); ++pfCandidate ) {
    if(deltaR2(iCand->p4(),pfCandidate->p4()) > iDR2Max)  continue;
    double dZ = -999.; // PH: If no vertex is reconstructed in the event
                       //     or PFCandidate has no track, set dZ to -999
    if ( hardScatterVertex ) {
      if      ( pfCandidate->trackRef().isNonnull()    ) dZ = std::abs(pfCandidate->trackRef()->dz(hardScatterVertex->position()));
      else if ( pfCandidate->gsfTrackRef().isNonnull() ) dZ = std::abs(pfCandidate->gsfTrackRef()->dz(hardScatterVertex->position()));
    }
    if(std::abs(dZ) > 0.1) continue; 
    lVis += pfCandidate->p4();
  }
  return lVis.pt()/iCand->pt();
}

#include "FWCore/Framework/interface/MakerMacros.h"

DEFINE_FWK_MODULE(PFMETProducerMVAInputPairs);
