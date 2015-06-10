// -*- C++ -*-

// Package:    PixelLumiDQM
// Class:      PixelLumiDQM

// Author: Amita Raval
// Based on Jeroen Hegeman's code for Pixel Cluster Count Luminosity

#include "PixelLumiDQM.h"

#include <memory>
#include <string>
#include <sstream>

#include "FWCore/Utilities/interface/EDMException.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Framework/interface/Run.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/LuminosityBlock.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "DataFormats/GeometryVector/interface/Pi.h"
#include "DataFormats/GeometryVector/interface/LocalPoint.h"
#include "DataFormats/GeometryVector/interface/GlobalPoint.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"
#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/SiPixelDetId/interface/PixelSubdetector.h"
#include "DataFormats/SiPixelDetId/interface/PixelBarrelName.h"
#include "DataFormats/SiPixelDetId/interface/PixelEndcapName.h"
#include "DataFormats/SiPixelCluster/interface/SiPixelCluster.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/Common/interface/DetSetNew.h"
#include "DataFormats/Common/interface/DetSetVectorNew.h"
#include "Geometry/CommonTopologies/interface/PixelTopology.h"
#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeometry.h"
#include "Geometry/Records/interface/TrackerDigiGeometryRecord.h"
#include "Geometry/TrackerGeometryBuilder/interface/PixelGeomDetUnit.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "DQMServices/Core/interface/MonitorElement.h"

//DIP lumi stuff
#include "RecoLuminosity/LumiProducer/interface/DIPLumiSummary.h"
#include "RecoLuminosity/LumiProducer/interface/DIPLumiDetail.h"
#include "RecoLuminosity/LumiProducer/interface/DIPLuminosityRcd.h"
#include "TBrowser.h"

//Filling scheme stuff

#include "DataFormats/L1GlobalTrigger/interface/L1GlobalTriggerEvmReadoutRecord.h"

//Timestamp stuff

#include <time.h>
#include <sys/time.h>

// Constants, enums and typedefs.
// None

// Static data member definitions.
// None

// Constructors and destructor.
PixelLumiDQM::PixelLumiDQM(const edm::ParameterSet& iConfig) :
  fVtxLabel(iConfig.getUntrackedParameter<edm::InputTag>("vertexLabel",
                                                         edm::InputTag("offlinePrimaryVertices"))),
  fPixelClusterLabel(iConfig.getUntrackedParameter<edm::InputTag>("pixelClusterLabel",
                                                                  edm::InputTag("siPixelClusters"))),
  fGtEvmLabel(iConfig.getUntrackedParameter<edm::InputTag>("gtEvmLabel",
                                                                  edm::InputTag("gtEvmDigis"))),

  fIncludeVertexInfo(iConfig.getUntrackedParameter<bool>("includeVertexInfo", true)),
  fIncludePixelClusterInfo(iConfig.getUntrackedParameter<bool>("includePixelClusterInfo", true)),
  fIncludePixelQualCheckHistos(iConfig.getUntrackedParameter<bool>("includePixelQualCheckHistos", true)),
  fResetIntervalInLumiSections(iConfig.getUntrackedParameter<int>("resetEveryNLumiSections", 1)),
  fDeadModules(iConfig.getUntrackedParameter<std::vector<uint32_t> >("deadModules", std::vector<uint32_t>())),
  fMinPixelsPerCluster(iConfig.getUntrackedParameter<int>("minNumPixelsPerCluster", 0)),
  fMinClusterCharge(iConfig.getUntrackedParameter<double>("minChargePerCluster", 0)),
  fIntActiveCrossingsFromDB((MonitorElement*)0),
  fIntLhcFillNumber((MonitorElement*)0),
  fStringLhcBeamMode((MonitorElement*)0),
  fHistnBClusVsLS(3,(MonitorElement*)0),
  fHistnFPClusVsLS(2,(MonitorElement*)0),
  fHistnFMClusVsLS(2,(MonitorElement*)0),
  fHistBunchCrossings((MonitorElement*)0),
  fHistBunchCrossingsLastLumi((MonitorElement*)0),
  fHistClusterCountByBxLastLumi((MonitorElement*)0),
  fLumiByBxPerLS(lastBunchCrossing,(MonitorElement*)0),
  fHistClusByLS((MonitorElement*)0),
  fHistTotalRecordedLumiByLS((MonitorElement*)0),
  fHistRecordedByBxLastLumi((MonitorElement*)0),
  fHistRecordedByBxCumulative((MonitorElement*)0),
  fHistHFDeliveredByLS((MonitorElement*)0),
  fHistHFRecordedByLS((MonitorElement*)0),
  fHistHFToPxRecordedRatioByLS((MonitorElement*)0),
  fHistHFToPxDeliveredRatioByLS((MonitorElement*)0),
  mylabel(iConfig.getParameter<std::string>("@@module_label")),
  intHFLumi(0.),
  intHFLumiByBx(lastBunchCrossing+1,0.),
  bunchTriggerMask(lastBunchCrossing+1,false),
  filledAndUnmaskedBunches(0),
  bunchFillMask(lastBunchCrossing+1,false),
  activeCrossingsFromHF(0),
  activeCrossingsFromDB(0),
  useInnerBarrelLayer(iConfig.getUntrackedParameter<bool>("useInnerBarrelLayer", false)),
  fOracleDB_(iConfig.getParameter<std::string>("oracleDB")),
  fPathCondDB_(iConfig.getParameter<std::string>("pathCondDB")),
  fFillNumber(0),
  fLogFileName_(iConfig.getUntrackedParameter<std::string>("logFileName","/tmp/pixel_lumi.txt")),
  interactive(iConfig.getUntrackedParameter<bool>("interactive", true)),
  newLS(true),
  lastGoodRetrievedLumiFromHF(0)
{
  edm::LogInfo("Configuration")
    << "PixelLumiDQM looking for pixel clusters in '"
    << fPixelClusterLabel << "'";
  edm::LogInfo("Configuration")
    << "PixelLumiDQM storing vertex info? "
    << fIncludeVertexInfo;
  edm::LogInfo("Configuration")
    << "PixelLumiDQM storing pixel cluster info? "
    << fIncludePixelClusterInfo;
  edm::LogInfo("Configuration")
    << "PixelLumiDQM storing pixel cluster quality check histograms? "
    << fIncludePixelQualCheckHistos;

  if (fIncludePixelClusterInfo) {
    if (!fDeadModules.size()) {
      edm::LogInfo("Configuration")
        << "No pixel modules specified to be ignored";
    } else {
      edm::LogInfo("Configuration")
        << fDeadModules.size() << " pixel modules specified to be ignored:";
      for (std::vector<uint32_t>::const_iterator it = fDeadModules.begin();
           it != fDeadModules.end(); ++it) {
        edm::LogInfo("Configuration")
          << "  " << *it;
      }
    }
    edm::LogInfo("Configuration")
      << "Ignoring pixel clusters with less than "
      << fMinPixelsPerCluster << " pixels";
    edm::LogInfo("Configuration")
      << "Ignoring pixel clusters with charge less than "
      << fMinClusterCharge;
  }
  if(interactive) new TBrowser("pixellumi");
}

PixelLumiDQM::~PixelLumiDQM()
{
  // Do anything here that needs to be done at desctruction time
  // (e.g. close files, deallocate resources etc.).
}


// Member functions.

// ---------- Method which fills 'descriptions' with the allowed parameters for the module.  ----------
void
PixelLumiDQM::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  // The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters.
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}


// ---------- Method called for each event.  ----------
void
PixelLumiDQM::analyze(const edm::Event& iEvent,
		      const edm::EventSetup& iSetup)
{
  // Collect all bookkeeping information.
  fRunNo = iEvent.id().run();
  fEvtNo = iEvent.id().event();
  fLSNo = iEvent.getLuminosityBlock().luminosityBlock();
  fBXNo = iEvent.bunchCrossing();
  fTimestamp = iEvent.time().unixTime();
  fHistBunchCrossings->Fill(float(fBXNo)); 
  fHistBunchCrossingsLastLumi->Fill(float(fBXNo)); 
  // This serves as event counter to compute luminosity from cluster counts.
  std::map<int, PixelClusterCount>::iterator it = fNumPixelClusters.find(fBXNo);
  if(it==fNumPixelClusters.end()) fNumPixelClusters[fBXNo] = PixelClusterCount();

  // get the fill number and beam mode from gtfe-- only check every new LS
  if(newLS){
    std::cout << "retrieving beam mode and fill number from GT " << std::endl;
//     edm::Handle<L1GlobalTriggerEvmReadoutRecord> gtEvmReadoutRecord;
//     iEvent.getByLabel(fGtEvmLabel,gtEvmReadoutRecord);
//     if(gtEvmReadoutRecord.isValid()){
//       std::cout << "gt evm record found valid " << std::endl;
//       const L1GtfeExtWord& gtfeEvmWord = gtEvmReadoutRecord->gtfeWord();
//       fBeamMode = gtfeEvmWord.beamMode();
//       if(fFillNumber == 0){
// 	fFillNumber = gtfeEvmWord.lhcFillNumber();
// 	fIntLhcFillNumber->Fill(fFillNumber);
// 	fStringLhcBeamMode->Fill(fBeamMode);
//       }
    if(activeCrossingsFromDB == 0){

      int conError;
      L1TOMDSHelper myOMDSHelper;
      myOMDSHelper.connect(fOracleDB_,fPathCondDB_,conError);
      if(conError != L1TOMDSHelper::NO_ERROR){
	std::string eName = myOMDSHelper.enumToStringError(conError);
	edm::LogError("DBConnection failure") << eName;
      }
	
      int retrieveError;
      fFillNumber = myOMDSHelper.getFillNumber(fRunNo,fLSNo,retrieveError);
      if(retrieveError != L1TOMDSHelper::NO_ERROR){
      std::string eName = myOMDSHelper.enumToStringError(retrieveError);
      std::cout << "fill:: Error in retrieving fill number " 
		<< eName << std::endl;
      }
      fIntLhcFillNumber->Fill(fFillNumber);
      // 	fStringLhcBeamMode->Fill(fBeamMode);

      activeCrossingsFromDB = 
	myOMDSHelper.getNumberCollidingBunches(fFillNumber,retrieveError);
      if(retrieveError != L1TOMDSHelper::NO_ERROR){
	std::string eName = myOMDSHelper.enumToStringError(retrieveError);
	std::cout << "nbxs:: Error in retrieving active crossings " 
		  << eName << std::endl;
      }
      fIntActiveCrossingsFromDB->Fill(activeCrossingsFromDB);
    }
  //     }
//     else 
//       std::cout << "gt evm record NOT valid " << std::endl;
    std::cout << "fill number " << fFillNumber << " beam mode " << fBeamMode 
	      << "active crossings " << activeCrossingsFromDB << std::endl; 
    newLS=false;
  }



  
  // ----------

  // Add the vertex info if requested (otherwise use recognizable  defaults).
  fNumVtx = 0;
  fNumVtxGood = 0;
  fNumTrkPerVtx = 0;
  fVtxX = -9999.;
  fVtxY = -9999.;
  fVtxZ = -9999.;
  fChi2 = -9999.;
  fNdof = -9999.;
  
  if (fIncludeVertexInfo) {
    edm::Handle<reco::VertexCollection> recoVertices;
    iEvent.getByLabel(fVtxLabel, recoVertices);
    
    if (recoVertices.isValid()) {
      for (reco::VertexCollection::const_iterator v = recoVertices->begin();
           v != recoVertices->end(); ++v) {
        double nDof = v->ndof();
        // Hard-coded (but standard) quality cuts!
        if (v->isValid() && !v->isFake() && (nDof > 4.)) {
          ++fNumVtx;
          unsigned int nTrk = v->tracksSize();
          if (nTrk > 0) {
            ++fNumVtxGood;
            if (nTrk > fNumTrkPerVtx) {
              // Keep track of the maximum number of tracks
	      // associated with any vertex in this event.
              fNumTrkPerVtx = nTrk;
              // And keep track of the highest-multiplicity vertex in this event.
              // NOTE: The magic scale factors convert everything to cm.
              fVtxX = 1.e4 * v->x();
              fVtxY = 1.e4 * v->y();
              fVtxZ = 10. * v->z();
              fChi2 = v->chi2();
              fNdof = v->ndof();
            }
          }
        }
      }
    } else {
      edm::LogError("NotFound")
        << "Could not find product '" << fVtxLabel << "'";
    }
  }
  // ----------


  // Add pixel cluster info if requested.
  // fNumPixelClusters.Reset(); AR: will reset at the end since I want to accumulate count over LS. 

  if (fIncludePixelClusterInfo) {
    // Find tracker geometry.
    edm::ESHandle<TrackerGeometry> trackerGeo;
    iSetup.get<TrackerDigiGeometryRecord>().get(trackerGeo);
    
    // Find pixel clusters.
    edm::Handle<edmNew::DetSetVector<SiPixelCluster> > pixelClusters;
    iEvent.getByLabel(fPixelClusterLabel, pixelClusters);
    
    // Loop over entire tracker geometry.
    for (TrackerGeometry::DetContainer::const_iterator
           i = trackerGeo->dets().begin();
         i != trackerGeo->dets().end(); ++i) {
      // See if this is a pixel unit(?).
         
      if ( GeomDetEnumerators::isTrackerPixel((*i)->subDetector())) {
        DetId detId = (*i)->geographicalId();
        // Find all clusters on this detector module.
        edmNew::DetSetVector<SiPixelCluster>::const_iterator iSearch =
          pixelClusters->find(detId);
        if (iSearch != pixelClusters->end()) {
	  
          // Count the number of clusters with at least a minimum
          // number of pixels per cluster and at least a minimum charge.
          size_t numClusters = 0;
          for (edmNew::DetSet<SiPixelCluster>::const_iterator
                 itClus = iSearch->begin();
               itClus != iSearch->end(); ++itClus) {
            if ((itClus->size() >= fMinPixelsPerCluster) &&
                (itClus->charge() >= fMinClusterCharge)) {
              ++numClusters;
            }
          }
	  // DEBUG DEBUG DEBUG
          assert(numClusters <= iSearch->size());
          // DEBUG DEBUG DEBUG end
	  
          // Add up the cluster count based on the position of this detector element.
	  if (detId.subdetId() == PixelSubdetector::PixelBarrel) {
            PixelBarrelName detName = PixelBarrelName(detId);
            int layer = detName.layerName();
	    fNumPixelClusters[fBXNo].numB.at(layer - 1) += numClusters;
	    fNumPixelClusters[fBXNo].dnumB.at(layer - 1) += sqrt(numClusters);
	  } else {
            // DEBUG DEBUG DEBUG
            assert(detId.subdetId() == PixelSubdetector::PixelEndcap);
            // DEBUG DEBUG DEBUG end
	    
            PixelEndcapName detName = PixelEndcapName(detId);
            PixelEndcapName::HalfCylinder halfCylinder = detName.halfCylinder();
            int disk = detName.diskName();
            switch (halfCylinder) {
            case PixelEndcapName::mO:
            case PixelEndcapName::mI:
              fNumPixelClusters[fBXNo].numFM.at(disk - 1) += numClusters;
              fNumPixelClusters[fBXNo].dnumFM.at(disk - 1) += sqrt(numClusters);
              break;
            case PixelEndcapName::pO:
            case PixelEndcapName::pI:
              fNumPixelClusters[fBXNo].numFP.at(disk - 1) += numClusters;
              fNumPixelClusters[fBXNo].dnumFP.at(disk - 1) += sqrt(numClusters);
              break;
            default:
              assert(false);
              break;
            }
          }
	  
        }
      }
    }
  }
  // ----------

  // Fill some pixel cluster quality check histograms if requested.
  if (fIncludePixelQualCheckHistos) {
    
    // Find tracker geometry.
    edm::ESHandle<TrackerGeometry> trackerGeo;
    iSetup.get<TrackerDigiGeometryRecord>().get(trackerGeo);
    
    // Find pixel clusters.
    edm::Handle<edmNew::DetSetVector<SiPixelCluster> > pixelClusters;
    iEvent.getByLabel(fPixelClusterLabel, pixelClusters);
    
    bool filterDeadModules = (fDeadModules.size() > 0);
    std::vector<uint32_t>::const_iterator deadModulesBegin = fDeadModules.begin();
    std::vector<uint32_t>::const_iterator deadModulesEnd = fDeadModules.end();
    
    // Loop over entire tracker geometry.
    for (TrackerGeometry::DetContainer::const_iterator
           i = trackerGeo->dets().begin();
         i != trackerGeo->dets().end(); ++i) {
      
      // See if this is a pixel module.
      if ( GeomDetEnumerators::isTrackerPixel((*i)->subDetector())) {
        DetId detId = (*i)->geographicalId();
	
        // Skip this module if it's on the list of modules to be ignored.
        if (filterDeadModules &&
            find(deadModulesBegin, deadModulesEnd, detId()) != deadModulesEnd) {
          continue;
        }
	
        // Find all clusters in this module.
        edmNew::DetSetVector<SiPixelCluster>::const_iterator iSearch =
          pixelClusters->find(detId);
	
        // Loop over all clusters in this module.
        if (iSearch != pixelClusters->end()) {
          for (edmNew::DetSet<SiPixelCluster>::const_iterator clus = iSearch->begin();
               clus != iSearch->end(); ++clus) {
	    
            if ((clus->size() >= fMinPixelsPerCluster) &&
                (clus->charge() >= fMinClusterCharge)) {
	      
              PixelGeomDetUnit const* theGeomDet =
                dynamic_cast<PixelGeomDetUnit const*>(trackerGeo->idToDet(detId));
              PixelTopology const* topol = &(theGeomDet->specificTopology());
              double x = clus->x();
              double y = clus->y();
              LocalPoint clustLP = topol->localPosition(MeasurementPoint(x, y));
              GlobalPoint clustGP = theGeomDet->surface().toGlobal(clustLP);
              double charge = clus->charge() / 1.e3;
              int size = clus->size();
	      
              if (detId.subdetId() == PixelSubdetector::PixelBarrel) {
                PixelBarrelName detName = PixelBarrelName(detId);
                int layer = detName.layerName();
                switch (layer) {
                case 1:
                  fHistContainerThisRun["clusPosBarrel1"]->Fill(clustGP.z(), clustGP.phi());
                  fHistContainerThisRun["clusChargeBarrel1"]->Fill(iEvent.bunchCrossing(), charge);
                  fHistContainerThisRun["clusSizeBarrel1"]->Fill(iEvent.bunchCrossing(), size);
                  break;
                case 2:
                  fHistContainerThisRun["clusPosBarrel2"]->Fill(clustGP.z(), clustGP.phi());
                  fHistContainerThisRun["clusChargeBarrel2"]->Fill(iEvent.bunchCrossing(), charge);
                  fHistContainerThisRun["clusSizeBarrel2"]->Fill(iEvent.bunchCrossing(), size);
                  break;
                case 3:
                  fHistContainerThisRun["clusPosBarrel3"]->Fill(clustGP.z(), clustGP.phi());
                  fHistContainerThisRun["clusChargeBarrel3"]->Fill(iEvent.bunchCrossing(), charge);
                  fHistContainerThisRun["clusSizeBarrel3"]->Fill(iEvent.bunchCrossing(), size);
                  break;
                default:
                  assert(false);
                  break;
                }
              } else {
                
		// DEBUG DEBUG DEBUG
                assert(detId.subdetId() == PixelSubdetector::PixelEndcap);
                // DEBUG DEBUG DEBUG end
                
		PixelEndcapName detName = PixelEndcapName(detId);
                PixelEndcapName::HalfCylinder halfCylinder = detName.halfCylinder();
                int disk = detName.diskName();
                switch (halfCylinder) {
                case PixelEndcapName::mO:
                case PixelEndcapName::mI:
                  switch (disk) {
                  case 1:
                    fHistContainerThisRun["clusPosEndCapM1"]->Fill(clustGP.x(), clustGP.y());
                    fHistContainerThisRun["clusChargeEndCapM1"]->Fill(iEvent.bunchCrossing(), charge);
                    fHistContainerThisRun["clusSizeEndCapM1"]->Fill(iEvent.bunchCrossing(), size);
                    break;
                  case 2:
                    fHistContainerThisRun["clusPosEndCapM2"]->Fill(clustGP.x(), clustGP.y());
                    fHistContainerThisRun["clusChargeEndCapM2"]->Fill(iEvent.bunchCrossing(), charge);
                    fHistContainerThisRun["clusSizeEndCapM2"]->Fill(iEvent.bunchCrossing(), size);
                    break;
                  default:
                    assert(false);
                    break;
                  }
                  break;
                case PixelEndcapName::pO:
                case PixelEndcapName::pI:
                  switch (disk) {
                  case 1:
                    fHistContainerThisRun["clusPosEndCapP1"]->Fill(clustGP.x(), clustGP.y());
                    fHistContainerThisRun["clusChargeEndCapP1"]->Fill(iEvent.bunchCrossing(), charge);
                    fHistContainerThisRun["clusSizeEndCapP1"]->Fill(iEvent.bunchCrossing(), size);
                    break;
                  case 2:
                    fHistContainerThisRun["clusPosEndCapP2"]->Fill(clustGP.x(), clustGP.y());
                    fHistContainerThisRun["clusChargeEndCapP2"]->Fill(iEvent.bunchCrossing(), charge);
                    fHistContainerThisRun["clusSizeEndCapP2"]->Fill(iEvent.bunchCrossing(), size);
                    break;
                  default:
                    assert(false);
                    break;
                  }
                  break;
                default:
                  assert(false);
                  break;
                }
              }
            }
          }
        }
      }
    }
  }
} // analyze()

// ---------- Method called once per job just before starting event loop.  ----------
// ---------- Method called once per job just after ending the event loop.  ----------
// ---------- Method called when starting to process a run.  ----------
void
PixelLumiDQM::bookHistograms(DQMStore::IBooker & ibooker, 
                                  edm::Run const & run,
                                  edm::EventSetup const & /* iSetup */)
{


  edm::LogInfo("Status") << "Starting processing of run #" << run.id().run();

  // Top folder containing high-level information about pixel and HF lumi.  
  std::string folder = "PixelLumi/";
  folder+=mylabel;
  ibooker.setCurrentFolder(folder);
  
  // Set binning and limits to reflect the actual bunch crossings
  fIntActiveCrossingsFromDB = ibooker.bookInt("activeCrossingsFromDB");
  fIntLhcFillNumber = ibooker.bookInt("lhcFillNumber");
  fStringLhcBeamMode = ibooker.bookString("lhcBeamMode", "LHC Beam Mode");
  fHistTotalRecordedLumiByLS = ibooker.book1D("totalPixelLumiByLS","Pixel Lumi in nb vs LS",8000,0.5,8000.5);
  fHistRecordedByBxCumulative = ibooker.book1D("PXLumiByBXsum","Pixel Lumi in nb by BX Cumulative vs LS",lastBunchCrossing,
					     0.5,float(lastBunchCrossing)+0.5);
  // AR: Abandoned this as by-BX query to HF DB (runtime_logger) is terribly slow!
  //fHistHFToPxRecordedRatioByBxCumulative = ibooker.book1D("HFRecordedToPXLumiByBxSum","Ratio of HF Recorded to PX Lumi By BX Cumulative",lastBunchCrossing,
  //0.5,float(lastBunchCrossing)+0.5);
  //fHistHFToPxDeliveredRatioByBxCumulative = ibooker.book1D("HFDeliveredToPXLumiByBxSum","Ratio of HF Delivered to PX Lumi By BX Cumulative",lastBunchCrossing,
  //0.5,float(lastBunchCrossing)+0.5);
  fHistHFToPxRecordedRatioByLS = ibooker.book1D("HFRecordedToPXByLS","Ratio of Recorded HF to PX vs LS",8000,0.5,8000.5);
  fHistHFToPxDeliveredRatioByLS = ibooker.book1D("HFDeliveredToPXByLS","Ratio of Delivered HF to PX vs LS",8000,0.5,8000.5);

  // Last LS or groups of LS histograms in a subfolder. AR: Not filling these 2 histograms. 
  std::string subfolder = folder + "/lastLS";
  ibooker.setCurrentFolder(subfolder);
  fHistRecordedByBxLastLumi = ibooker.book1D("PXByBXLastLumi","Pixel By BX Last Lumi",lastBunchCrossing+1,
					   -0.5,float(lastBunchCrossing)+0.5);
  //fHistHFToPxRecordedRatioByBxLastLS = ibooker.book1D("HFRecordedToPXByBxLastLumi","Ratio of HF Recorded to PX By BX Last Lumi",lastBunchCrossing,
  //						    0.5,float(lastBunchCrossing)+0.5);
  //fHistHFToPxDeliveredRatioByBxLastLS = ibooker.book1D("HFDeliveredToPXByBxLastLumi","Ratio of HF Delivered to PX By BX Last Lumi",lastBunchCrossing,
  //0.5,float(lastBunchCrossing)+0.5);
  

  // Details about cluster counting go here 
  subfolder = folder+"/ClusterCountingDetails";
  ibooker.setCurrentFolder(subfolder);

  fHistnBClusVsLS[0] = ibooker.book1D("nBClusVsLS_0","Fraction of Clusters vs LS Barrel layer 0",8000,0.5,8000.5);
  fHistnBClusVsLS[1] = ibooker.book1D("nBClusVsLS_1","Fraction of Clusters vs LS Barrel layer 1",8000,0.5,8000.5);
  fHistnBClusVsLS[2] = ibooker.book1D("nBClusVsLS_2","Fraction of Clusters vs LS Barrel layer 2",8000,0.5,8000.5);
  fHistnFPClusVsLS[0] = ibooker.book1D("nFPClusVsLS_0","Fraction of Clusters vs LS Barrel layer 0",8000,0.5,8000.5);
  fHistnFPClusVsLS[1] = ibooker.book1D("nFPClusVsLS_1","Fraction of Clusters vs LS Barrel layer 1",8000,0.5,8000.5);
  fHistnFMClusVsLS[0] = ibooker.book1D("nFMClusVsLS_0","Fraction of Clusters vs LS Barrel layer 0",8000,0.5,8000.5);
  fHistnFMClusVsLS[1] = ibooker.book1D("nFMClusVsLS_1","Fraction of Clusters vs LS Barrel layer 1",8000,0.5,8000.5);
  fHistBunchCrossings = ibooker.book1D("BunchCrossings","Cumulative Bunch Crossings",lastBunchCrossing,
				 0.5,float(lastBunchCrossing)+0.5);
  fHistBunchCrossingsLastLumi = ibooker.book1D("BunchCrossingsLL","Bunch Crossings Last Lumi",lastBunchCrossing,
					 0.5,float(lastBunchCrossing)+0.5);
  fHistClusterCountByBxLastLumi = ibooker.book1D("ClusterCountByBxLL","Cluster Count by BX Last Lumi",lastBunchCrossing,
					 0.5,float(lastBunchCrossing)+0.5);
  fHistClusterCountByBxCumulative = ibooker.book1D("ClusterCountByBxSum","Cluster Count by BX Cumulative",lastBunchCrossing,
					 0.5,float(lastBunchCrossing)+0.5);
  fHistClusByLS = ibooker.book1D("totalClusByLS","Number of Clusters all dets vs LS",8000,0.5,8000.5);

  // Add some pixel cluster quality check histograms (in a subfolder).
  subfolder = folder+"/qualityChecks";
  ibooker.setCurrentFolder(subfolder);
  
  if (fIncludePixelQualCheckHistos) {
    
    if(!(&ibooker)) {
      throw edm::Exception(edm::errors::Configuration,
                           "TFileService is not registered in the cfg file");
    }
    
    // Create histograms for this run if not already present in our list.
      edm::LogInfo("Status") << "Creating histograms for run #" << run.id().run();


      //----------
      
      // Pixel cluster positions in the barrel - (z, phi).
      for (size_t i = 1; i <= kNumLayers; ++i) {
        std::stringstream key;
        key << "clusPosBarrel" << i;
        std::stringstream name;
        name << key.str() << "_" << run.run();
        std::stringstream title;
        title << "Pixel cluster position - barrel layer " << i;
        MonitorElement* hist = ibooker.book2D(name.str().c_str(),
					    title.str().c_str(),
					    100, -30., 30.,
					    64, -Geom::pi(), Geom::pi());
	// hist->Sumw2();
	fHistContainerThisRun[key.str()] = hist;
      }
      
      // Pixel cluster positions in the endcaps (x, y).
      std::vector<std::string> sides;
      sides.push_back("M");
      sides.push_back("P");
      for (std::vector<std::string>::const_iterator side = sides.begin();
           side != sides.end(); ++side) {
        for (size_t i = 1; i <= kNumDisks; ++i) {
          std::stringstream key;
          key << "clusPosEndCap" << *side << i;
          std::stringstream name;
          name << key.str() << "_" << run.run();
          std::stringstream title;
          title << "Pixel cluster position - endcap disk " << i;
          MonitorElement* hist = ibooker.book2D(name.str().c_str(),
					      title.str().c_str(),
					      100, -20., 20.,
					      100, -20., 20.);
	  // hist->Sumw2();
	  fHistContainerThisRun[key.str()] = hist;
        }
      }
      
      //----------
      
      // Pixel cluster charge in the barrel, per bx.
      for (size_t i = 1; i <= kNumLayers; ++i) {
        std::stringstream key;
        key << "clusChargeBarrel" << i;
        std::stringstream name;
        name << key.str() << "_" << run.run();
        std::stringstream title;
        title << "Pixel cluster charge - barrel layer " << i;
        MonitorElement* hist = ibooker.book2D(name.str().c_str(),
					    title.str().c_str(),
					    3564, .5, 3564.5,
					    100, 0., 100.);
	// hist->Sumw2();
	fHistContainerThisRun[key.str()] = hist;
      }

      // Pixel cluster charge in the endcaps, per bx.
      for (std::vector<std::string>::const_iterator side = sides.begin();
           side != sides.end(); ++side) {
        for (size_t i = 1; i <= kNumDisks; ++i) {
          std::stringstream key;
          key << "clusChargeEndCap" << *side << i;
          std::stringstream name;
          name << key.str() << "_" << run.run();
          std::stringstream title;
          title << "Pixel cluster charge - endcap disk " << i;
          MonitorElement* hist = ibooker.book2D(name.str().c_str(),
                                      title.str().c_str(),
                                      3564, .5, 3564.5,
                                      100, 0., 100.);
	  // hist->Sumw2();
           fHistContainerThisRun[key.str()] = hist;
        }
      }

      //----------

      // Pixel cluster size in the barrel, per bx.
      for (size_t i = 1; i <= kNumLayers; ++i) {
        std::stringstream key;
        key << "clusSizeBarrel" << i;
        std::stringstream name;
        name << key.str() << "_" << run.run();
        std::stringstream title;
        title << "Pixel cluster size - barrel layer " << i;
        MonitorElement* hist = ibooker.book2D(name.str().c_str(),
                                    title.str().c_str(),
                                    3564, .5, 3564.5,
                                    100, 0., 100.);
	// hist->Sumw2();
         fHistContainerThisRun[key.str()] = hist;
      }

      // Pixel cluster size in the endcaps, per bx.
      for (std::vector<std::string>::const_iterator side = sides.begin();
           side != sides.end(); ++side) {
        for (size_t i = 1; i <= kNumDisks; ++i) {
          std::stringstream key;
          key << "clusSizeEndCap" << *side << i;
          std::stringstream name;
          name << key.str() << "_" << run.run();
          std::stringstream title;
          title << "Pixel cluster size - endcap disk " << i;
          MonitorElement* hist = ibooker.book2D(name.str().c_str(),
                                      title.str().c_str(),
                                      3564, .5, 3564.5,
                                      100, 0., 100.);
	  // hist->Sumw2();
           fHistContainerThisRun[key.str()] = hist;
        }
      }

      //----------


  }

  // Bunch-by-bunch detailed plots go in a subfolder too
  subfolder = folder+"/bunchByBunch";
  ibooker.setCurrentFolder(subfolder);

  for(unsigned int i = 0; i<lastBunchCrossing; i++){
    std::ostringstream ost;
    ost << "LumiByLSBX"<<i;
    //fLumiByBxPerLS[i] = ibooker.book1D(ost.str(),ost.str(),3000,0.5,3000.5);
  }

  // Book or get HF lumi histograms
  folder = "PixelLumi/HF";
  ibooker.setCurrentFolder(folder);

  if(fHistHFRecordedByLS==0)
    fHistHFRecordedByLS=ibooker.book1D("totalHFRecordedLumiByLS","Recorded HF Lumi in nb vs LS",8000,0.5,8000.5);

  if(fHistHFDeliveredByLS==0)
    fHistHFDeliveredByLS=ibooker.book1D("totalHFDeliveredLumiByLS","Delivered HF Lumi in nb vs LS",8000,0.5,8000.5);

  //fHistHFRecordedByBxCumulative = ibooker.get(folder+"/HFRecordedByBXsum");
  //if(fHistHFRecordedByBxCumulative==0)
  //fHistHFRecordedByBxCumulative = ibooker.book1D("HFRecordedByBXsum","HF Lumi Recorded By BX Cumulative in nb",lastBunchCrossing, 0.5,float(lastBunchCrossing)+0.5);
  //fHistHFDeliveredByBxCumulative = ibooker.get(folder+"/HFDeliveredByBXsum");
  //if(fHistHFDeliveredByBxCumulative==0)
  //fHistHFDeliveredByBxCumulative = ibooker.book1D("HFDeliveredByBXsum","HF Delivered By BX Cumulative in nb",lastBunchCrossing, 0.5,float(lastBunchCrossing)+0.5);

  //fHistHFRecordedByBxLastLS = ibooker.get(folder+"/HFRecordedByBXLastLumi");
  //if(fHistHFRecordedByBxLastLS==0)
  //fHistHFRecordedByBxLastLS = ibooker.book1D("HFRecordedByBXLastLumi","HF Lumi Recorded By BX Last Lumi in nb",lastBunchCrossing,0.5,float(lastBunchCrossing)+0.5);
  //fHistHFDeliveredByBxLastLS = ibooker.get(folder+"/HFDeliveredByBXLastLumi");
  //if(fHistHFDeliveredByBxLastLS==0)
  //fHistHFDeliveredByBxLastLS = ibooker.book1D("HFDeliveredByBXLastLumi","HF Lumi Delivered By BX Last Lumi in nb",lastBunchCrossing,0.5,float(lastBunchCrossing)+0.5);
  
}

// ------------ Method called when ending the processing of a run.  ------------
void
PixelLumiDQM::dqmBeginRun(edm::Run const&, edm::EventSetup const&)
{
}

// ------------ Method called when ending the processing of a run.  ------------
void
PixelLumiDQM::endRun(edm::Run const&, edm::EventSetup const&)
{
}


// ------------ Method called when starting to process a luminosity block.  ------------
void
PixelLumiDQM::beginLuminosityBlock(edm::LuminosityBlock const&lumiBlock,
				   edm::EventSetup const&)
{
  newLS = true;
  // Only reset and fill every fResetIntervalInLumiSections (default is 1 LS)
  // Return unless the PREVIOUS LS was at the right modulo value 
  // (e.g. is resetinterval = 5 the rest will only be executed at LS=6
  // NB: reset is done here so the histograms by LS are sent before resetting.
  // NB: not being used for now since default is 1 LS. There is a bug here.

  unsigned int ls = lumiBlock.luminosityBlockAuxiliary().luminosityBlock();

  if((ls-1)%fResetIntervalInLumiSections==0){
    fHistBunchCrossingsLastLumi->Reset();
    fHistClusterCountByBxLastLumi->Reset();
    fHistRecordedByBxLastLumi->Reset();
    //fHistHFRecordedByBxLastLS->Reset();
    //fHistHFToPxRecordedRatioByBxLastLS->Reset();
  }
}


// ------------ Method called when ending the processing of a luminosity block.  ------------
void
PixelLumiDQM::endLuminosityBlock(edm::LuminosityBlock const& lumiBlock,
				 edm::EventSetup const&es)
{
  
  unsigned int ls = lumiBlock.luminosityBlockAuxiliary().luminosityBlock();

  // retrieve HF luminosity for the latest available lumi section
  // this may not be the same as the currently ending ls so we fill the histograms here and will
  // calculate the ratios up to the ls for which valid HF values could be retrieved
  // lastGoodRetrievedLumiFromHF holds the information of the highest LS for which a query succeeded
  float intHFLumiDelivered = 0.;
  float intHFLumiRecorded = 0.;
  int retrievedLumi = ls; // note that after query to DB the retrievedLumi value will be replaced by the latest available LS value
                          // smaller or equal to ls
  int conError;
  L1TOMDSHelper myOMDSHelper;
  myOMDSHelper.connect(fOracleDB_,fPathCondDB_,conError);
  if(conError != L1TOMDSHelper::NO_ERROR){
    std::string eName = myOMDSHelper.enumToStringError(conError);
    edm::LogError("DBConnection failure") << eName;
  }
  
  int retrieveError = L1TOMDSHelper::NO_ERROR;
  intHFLumiDelivered = myOMDSHelper.getHFDeliveredLumi(fRunNo,retrievedLumi,retrieveError);
  if(retrieveError != L1TOMDSHelper::NO_ERROR){
    std::string eName = myOMDSHelper.enumToStringError(retrieveError);
    std::cout << "HFdelivered:: Error in retrieving  number " 
	      << eName << std::endl;
  }
  intHFLumiRecorded = myOMDSHelper.getHFRecordedLumi(fRunNo,retrievedLumi,retrieveError);
  if(retrieveError != L1TOMDSHelper::NO_ERROR){
    std::string eName = myOMDSHelper.enumToStringError(retrieveError);
    std::cout << "HFrecorded:: Error in retrieving  number " 
	      << eName << std::endl;
  }
  fHistHFRecordedByLS->setBinContent(retrievedLumi,intHFLumiRecorded);
  fHistHFDeliveredByLS->setBinContent(retrievedLumi,intHFLumiDelivered);          
  std::cout << "retrieved HF delivered lumi = " <<  intHFLumiDelivered
	    << " recorded lumi =" <<  intHFLumiRecorded 
	    << " for Lumi Section " << retrievedLumi 
	    << std::endl;
  //       if(intHFLumiDelivered != 0.)
  // 	fHistHFRecordedToDeliveredRatioByLS->setBinContent(ls,intHFLumiRecorded/intHFLumiDelivered);  
  
  
  // Only fill every fResetIntervalInLumiSections (default is 1 LS)
  if(ls%fResetIntervalInLumiSections!=0) return;
  
  printf("Lumi Block = %d\n",ls);

  if((ls-1)%fResetIntervalInLumiSections==0){
  }
  
  unsigned int nBClus[3] = {0,0,0};
  unsigned int nFPClus[2] = {0, 0};
  unsigned int nFMClus[2] = {0, 0};
  
  double total_recorded = 0.;
  double total_recorded_unc_square = 0.;
  

  // Obtain bunch-by-bunch cluster counts and compute totals for lumi calculation.
  double totalcounts = 0.d;
  double etotalcounts = 0.d;
  double totalevents = 0.d;
  double lumi_factor_per_bx = 0.d;
  if(useInnerBarrelLayer) 
    lumi_factor_per_bx = FREQ_ORBIT * SECONDS_PER_LS * fResetIntervalInLumiSections / XSEC_PIXEL_CLUSTER  ;
  else
    lumi_factor_per_bx = FREQ_ORBIT * SECONDS_PER_LS * fResetIntervalInLumiSections / rXSEC_PIXEL_CLUSTER  ;
  
  for(std::map<int, PixelClusterCount>::iterator it = fNumPixelClusters.begin();
      it != fNumPixelClusters.end(); it++) {
    
    // Sum all clusters for this BX.
    unsigned int total = (*it).second.numB.at(1)+
      (*it).second.numB.at(2)+(*it).second.numFP.at(0)+(*it).second.numFP.at(1)+
      (*it).second.numFM.at(0)+(*it).second.numFM.at(1);
    if(useInnerBarrelLayer) total+=(*it).second.numB.at(0);
    totalcounts += total;
    double etotal = (*it).second.dnumB.at(1)+
      (*it).second.dnumB.at(2)+(*it).second.dnumFP.at(0)+(*it).second.dnumFP.at(1)+
      (*it).second.dnumFM.at(0)+(*it).second.dnumFM.at(1);
    if(useInnerBarrelLayer) etotal = (*it).second.dnumB.at(0);
    etotalcounts += etotal;
    etotal = sqrt(etotal);
    // std::cout << "Filling cluster count for BX " << (*it).first << " total " << total << std::endl;
    fHistClusterCountByBxLastLumi->setBinContent((*it).first,total);
    fHistClusterCountByBxLastLumi->setBinError((*it).first,etotal);
    fHistClusterCountByBxCumulative->setBinContent((*it).first,fHistClusterCountByBxCumulative->getBinContent((*it).first)+total);
    
    // Extract number of events per BX (NB: bins are numbered from 0.5 so BX = n corresponds to bin n)
    // Calculate bunch-specific lumi (average over LS *or multiple LS !!!*, default 1 LS).
    // [lumi_factor_per_bx is multiplied by fResetIntervalInLumiSections in case values are not updated every LS] )
    unsigned int events_per_bx = fHistBunchCrossingsLastLumi->getBinContent((*it).first);
    totalevents += events_per_bx;
    double average_cluster_count = events_per_bx !=0 ? double(total)/events_per_bx : 0.;
    double average_cluster_count_unc = events_per_bx!=0 ? etotal/events_per_bx : 0.;
    double pixel_bx_lumi_per_ls =  lumi_factor_per_bx * average_cluster_count / CM2_TO_NANOBARN ;
    double pixel_bx_lumi_per_ls_unc = 0.d;
    if(useInnerBarrelLayer) 
      pixel_bx_lumi_per_ls_unc = sqrt(lumi_factor_per_bx*lumi_factor_per_bx *
				      (average_cluster_count_unc*average_cluster_count_unc +
				       (average_cluster_count*  XSEC_PIXEL_CLUSTER_UNC /
					XSEC_PIXEL_CLUSTER )*
				       (average_cluster_count*  XSEC_PIXEL_CLUSTER / 
					XSEC_PIXEL_CLUSTER ))
				      ) / CM2_TO_NANOBARN ;
    else
      pixel_bx_lumi_per_ls_unc = sqrt(lumi_factor_per_bx*lumi_factor_per_bx *
				      (average_cluster_count_unc*average_cluster_count_unc +
				       (average_cluster_count*  rXSEC_PIXEL_CLUSTER_UNC /
					rXSEC_PIXEL_CLUSTER )*
				       (average_cluster_count*  rXSEC_PIXEL_CLUSTER / 
					rXSEC_PIXEL_CLUSTER ))
				      ) / CM2_TO_NANOBARN ;
    
    //AR: do not fill LumiByBxPerLS to save on rate
    //fLumiByBxPerLS[(*it).first]->setBinContent(ls,pixel_bx_lumi_per_ls);
    //fLumiByBxPerLS[(*it).first]->setBinError(ls,pixel_bx_lumi_per_ls_unc);
    fHistRecordedByBxLastLumi->setBinContent((*it).first,pixel_bx_lumi_per_ls);
    fHistRecordedByBxLastLumi->setBinError((*it).first,pixel_bx_lumi_per_ls_unc);
    
    fHistRecordedByBxCumulative->setBinContent((*it).first,
					       fHistRecordedByBxCumulative->getBinContent((*it).first)+
					       pixel_bx_lumi_per_ls);
    
    /*
      if(fHistRecordedByBxLastLumi->getBinContent((*it).first)!=0.)
      fHistHFToPxRecordedRatioByBxLastLS->setBinContent((*it).first,
      fHistHFRecordedByBxLastLS->getBinContent((*it).first)/
      fHistRecordedByBxLastLumi->getBinContent((*it).first));
      if(fHistRecordedByBxCumulative->getBinContent((*it).first)!=0.)
      fHistHFToPxRecordedRatioByBxCumulative->setBinContent((*it).first,
      fHistHFRecordedByBxCumulative->getBinContent((*it).first)/
      fHistRecordedByBxCumulative->getBinContent((*it).first));
    */


    nBClus[0] +=(*it).second.numB.at(0);
    nBClus[1] +=(*it).second.numB.at(1);
    nBClus[2] +=(*it).second.numB.at(2);
    nFPClus[0] +=(*it).second.numFP.at(0);
    nFPClus[1] +=(*it).second.numFP.at(1);
    nFMClus[0] +=(*it).second.numFM.at(0);
    nFMClus[1] +=(*it).second.numFM.at(1);
    
    // Reset counters 
    (*it).second.Reset();

    // std::cout << "bx="<< (*it).first << " clusters=" << (*it).second.numB.at(0)) << std::endl; 
  }
  // totals:
  // Need to apply a filled bunch mask here - figure out how to get it 
  if((filledAndUnmaskedBunches = calculateBunchMask(fHistClusterCountByBxCumulative,bunchTriggerMask))!=0){
    for(unsigned int i = 0; i<= lastBunchCrossing; i++){
      // AR: not using the bunch mask for now as the HF is removed
      if(bunchTriggerMask[i]){ 
	// AR: I apply the BX fill mask here, assuming masked BXs will not contribute
	// NB: for random triggers the totals over all unmasked BXs don't make sense
	// Look at individual BXs to extract corrections
	
	double err = fHistRecordedByBxLastLumi->getBinError(i);
	total_recorded += fHistRecordedByBxLastLumi->getBinContent(i);
	total_recorded_unc_square += err*err;
      }
    }
    
    // Replace the total obtained by summing over BXs with the average per BX from the total cluster count and rescale 
    
    if(totalevents > 10){
      total_recorded = lumi_factor_per_bx * totalcounts / totalevents / CM2_TO_NANOBARN ;
    }
    else total_recorded = 0.d;
    
    
    // Rescale the pixel lumi to the ratio of unmasked to filled BXs. ----------
    // AR: not using the active crossings; being done by hand for the moment since 
    // the filling scheme info is not in the event, also rate too low to count filled bunch scheme and HF commented out.

    total_recorded *= double(activeCrossingsFromDB);///double(filledAndUnmaskedBunches));
    //total_recorded *= (/*double(activeCrossingsFromHF)*/1.d/double(filledAndUnmaskedBunches));
    //how to rescale the uncertainty ? here I simply assume it to scale as a sum of squares with the number of bunches 

    total_recorded_unc_square *= (double(activeCrossingsFromDB)/double(filledAndUnmaskedBunches));
    //total_recorded_unc_square *= (/*double(activeCrossingsFromHF)*/ 1.d/double(filledAndUnmaskedBunches));
    std::cout << " Total recorded " << total_recorded  << std::endl;
    fHistTotalRecordedLumiByLS->setBinContent(ls,total_recorded);
    fHistTotalRecordedLumiByLS->setBinError(ls,
					    sqrt(total_recorded_unc_square));
  }
  // fill cluster counts by detector regions for sanity checks
  unsigned int all_detectors_counts = 0;
  for(unsigned int i = 0; i < 3; i++){
    all_detectors_counts+=nBClus[i];
  }
  for(unsigned int i = 0; i < 2; i++){
    all_detectors_counts+=nFPClus[i];
  }
  for(unsigned int i = 0; i < 2; i++){
    all_detectors_counts+=nFMClus[i];
  }
  
  fHistClusByLS->setBinContent(ls, all_detectors_counts);
  
  for(unsigned int i = 0; i < 3; i++){
    fHistnBClusVsLS[i]->setBinContent(ls, 
				      float(nBClus[i])/float(all_detectors_counts));
  }
  for(unsigned int i = 0; i < 2; i++){
    fHistnFPClusVsLS[i]->setBinContent(ls, 
				       float(nFPClus[i])/float(all_detectors_counts));
  }
  for(unsigned int i = 0; i < 2; i++){
    fHistnFMClusVsLS[i]->setBinContent(ls, 
				       float(nFMClus[i])/float(all_detectors_counts));
  }
  
  // Fill ratio of HFRecorded and HFDelivered with PX total (if a new value of HF lumi became available).
  if(retrievedLumi!=0){

    for(int rls = lastGoodRetrievedLumiFromHF+1; rls <= retrievedLumi; rls++){
      if(fHistTotalRecordedLumiByLS->getBinContent(rls)!=0.){ // Avoid a divide-by-0 
	fHistHFToPxRecordedRatioByLS->setBinContent(rls,
						    fHistHFRecordedByLS->getBinContent(rls)/
						    fHistTotalRecordedLumiByLS->getBinContent(rls)/
						    0.98); // This is the Afterglow correction 
	fHistHFToPxDeliveredRatioByLS->setBinContent(rls,
						     fHistHFDeliveredByLS->getBinContent(rls)/
						     fHistTotalRecordedLumiByLS->getBinContent(rls)/
						     0.98); // This is the Afterglow correction 
	std::cout << "LUMISECTION::"<< rls  << " " << mylabel 
		  << " over "<< fResetIntervalInLumiSections << " LS HF_Rec to PX ratio " 
		  << fHistHFRecordedByLS->getBinContent(rls)/fHistTotalRecordedLumiByLS->getBinContent(rls)/0.98 
		  << " HF Recorded Lumi = " << fHistHFRecordedByLS->getBinContent(rls)
		  << " PX Lumi = " << fHistTotalRecordedLumiByLS->getBinContent(rls) << " unc = " 
		  << fHistTotalRecordedLumiByLS->getBinError(rls)
		  << std::endl;
      }
      else{
	fHistHFToPxRecordedRatioByLS->setBinContent(rls,-1.); // but mark situation clearly by giving ratio a value of -1.
	fHistHFToPxDeliveredRatioByLS->setBinContent(rls,-1.); // but mark situation clearly by giving ratio a value of -1.
      }
    }

    lastGoodRetrievedLumiFromHF = retrievedLumi; // now save the new value of last good retrieved LS
  }
  logFile_.open(fLogFileName_.c_str(),std::ios_base::trunc);		

  timeval tv;
  gettimeofday(&tv,0);
  tm *ts = gmtime(&tv.tv_sec);
  char datestring[256];
  strftime(datestring, sizeof(datestring),"%Y.%m.%d %T GMT %s",ts);
  logFile_ << "RunNumber "<< fRunNo << std::endl;
  logFile_ << "EndTimeOfFit " << datestring << std::endl;
  logFile_ << "LumiRange "<< lastGoodRetrievedLumiFromHF << "-" << lastGoodRetrievedLumiFromHF << std::endl;
  logFile_ << "Fill "<< fFillNumber << std::endl;
  logFile_ << "ActiveBunchCrossings "<< activeCrossingsFromDB << std::endl;
  logFile_ << "PixelLumi "<< fHistTotalRecordedLumiByLS->getBinContent(lastGoodRetrievedLumiFromHF) * 0.98 << std::endl;
  logFile_ << "HFLumi "<< fHistHFDeliveredByLS->getBinContent(lastGoodRetrievedLumiFromHF) << std::endl;
  logFile_ << "Ratio " << fHistHFToPxDeliveredRatioByLS->getBinContent(lastGoodRetrievedLumiFromHF) << std::endl;
  logFile_.close();
}

unsigned int PixelLumiDQM::calculateBunchMask(MonitorElement *e, std::vector<bool> &mask){
  unsigned int nbins = e->getNbinsX();
  std::vector<float> ar(nbins,0.);
  for(unsigned int i = 1; i<= nbins; i++){
    ar[i] = e->getBinContent(i);
  }
  return calculateBunchMask(ar,nbins,mask);
}
unsigned int PixelLumiDQM::calculateBunchMask(std::vector<float> &e, unsigned int nbins, std::vector<bool> &mask){
  // Take the cumulative cluster count histogram and find max and average of non-empty bins.
  unsigned int active_count = 0;
  double maxc = 0.d;
  double ave = 0.d; // Average of non-empty bins
  unsigned int non_empty_bins = 0;
  
  for(unsigned int i = 1; i<= nbins; i++){
    double bin = e[i];
    if(bin !=0.d){
      if(maxc<bin) maxc = bin;
      ave += bin;
      non_empty_bins++;
    }      
  }
  
  ave /= non_empty_bins;
  std::cout << "Bunch mask finder - non empty bins " << non_empty_bins
	    << " average of non empty bins " << ave 
	    << " max content of one bin " << maxc << std::endl;
  double mean = 0.;
  double sigma = 0.;
  if(non_empty_bins < 50){
    mean = maxc;
    sigma = (maxc)/20;
  }    
  else{
    TH1F dist("dist","dist",500,0.,maxc+(maxc/500.)*20.);
    for(unsigned int i = 1; i<= nbins; i++){
      double bin = e[i];
      dist.Fill(bin);
    }  
    dist.Fit("gaus","","",fmax(0.,ave-(maxc-ave)/5.),maxc);
    TF1 *fit = dist.GetFunction("gaus");
    mean = fit->GetParameter("Mean");
    sigma = fit->GetParameter("Sigma");
  }
  std::cout << "Bunch mask will use mean" << mean << " sigma " << sigma << std::endl;
  // Active BX defined as those which have nclus within fixed standard deviations of peak.
  for(unsigned int i = 1; i<= nbins; i++){
    double bin = e[i];
    if(bin>0. && abs(bin-mean)<5.*(sigma)){ mask[i]=true; active_count++;}
  }
  std::cout << "Bunch mask finds " << active_count << " active bunch crossings " << std::endl;
  //   std::cout << "this is the full bx mask " ;
  //   for(unsigned int i = 1; i<= nbins; i++)
  //     std::cout << ((mask[i]) ? 1:0);
  //   std::cout << std::endl;
  return active_count;
}
// Define this as a CMSSW plug-in.
DEFINE_FWK_MODULE(PixelLumiDQM);
