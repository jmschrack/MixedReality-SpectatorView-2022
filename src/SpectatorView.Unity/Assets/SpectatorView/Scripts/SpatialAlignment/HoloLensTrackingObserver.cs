// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma warning disable CS0618 // 'WorldManager' is obsolete: 'Support for built-in VR will be removed in Unity 2020.1.

using UnityEngine;

namespace Microsoft.MixedReality.SpectatorView
{
    /// <summary>
    /// MonoBehaviour that reports tracking information for a HoloLens device.
    /// </summary>
    public class HoloLensTrackingObserver : TrackingObserver
    {
                
        /// <inheritdoc/>
        public override TrackingState TrackingState
        {
            get
            {
#if UNITY_EDITOR
                return TrackingState.Tracking;
#elif UNITY_WSA 

#if UNITY_2020_2_OR_NEWER
                if(Microsoft.MixedReality.OpenXR.OpenXRContext.Current.IsSessionRunning)
#else
                if (UnityEngine.XR.WSA.WorldManager.state == UnityEngine.XR.WSA.PositionalLocatorState.Active)
#endif
                {
                    return TrackingState.Tracking;
                }
                return TrackingState.LostTracking;
#else
                return TrackingState.Unknown;
#endif
            }
        }
    }
}