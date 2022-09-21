using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.ARFoundation;
//Susing Microsoft.MixedReality.WorldLocking.Core;
//using UnityEngine.XR.ARSubsystems;
/* using Microsoft.MixedReality.ARSubsystems.XRAnchorStore;
using UnityEngine.XR.WindowsMR.XRAnchorStore;
using Microsoft.MixedReality.OpenXR.ARSubsystems;  */


namespace Microsoft.MixedReality.SpectatorView
{
    public class WorldAnchor : MonoBehaviour
    {
        public bool isLocated => false;// base.PinActive;//{ get; internal set; }
        bool lastState = false;
        public Action<WorldAnchor, bool> OnTrackingChanged;
        
        void Update(){
            //base.Manager.Save();
            if(lastState!=isLocated){
                lastState = isLocated;
                if(OnTrackingChanged!=null){
                    OnTrackingChanged(this, isLocated);
                }
            }
        }
    }
    
}