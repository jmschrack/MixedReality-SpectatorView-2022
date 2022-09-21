using System;
using System.Collections.Generic;
using UnityEngine;
using System.IO;
//using Microsoft.MixedReality.WorldLocking.Core;

namespace Microsoft.MixedReality.SpectatorView
{
	public class WorldAnchorStore : MonoBehaviour{
        static string dataStorePath;// = string.Format("{0}/holocamera.persist", Application.persistentDataPath);
        static WorldAnchorStore instance=null;
        static bool shuttingDown = false;
       // public WorldLockingManager manager;
        List<string> worldAnchorIDs=null;
        void Awake(){
			if(instance==null&&!shuttingDown){
                instance = this;
				dataStorePath = string.Format("{0}/holocamera.persist", Application.persistentDataPath);
                DontDestroyOnLoad(this);
            }else{
                Destroy(this);
            }
		}
		void OnDestroy(){
			if(instance==this){
                instance = null;
                shuttingDown = true;
            }
		}

        void Start(){
          // manager=WorldLockingManager.GetInstance();
        }

		public WorldAnchor Load(string id,GameObject target) {
            //XRAnchorStore a;
            //manager.AnchorManager.LoadAnchors();
            //manager.AlignmentManager.RestoreAlignmentAnchor(id, anchor);
            //manager.AlignmentManager.RestoreAlignmentAnchor();
            //return manager.AnchorManager.LoadAnchors();
			throw new NotImplementedException();
        }

		internal IEnumerable<string> GetAllIds()
		{
            return worldAnchorIDs;
            //throw new NotImplementedException();
		}

		internal void Save(string id, WorldAnchor anchor)
		{
			//manager.AlignmentManager.AddAlignmentAnchor()
			throw new NotImplementedException();
		}

		internal void Delete(string key)
		{
			throw new NotImplementedException();
		}

		internal void Dispose()
		{
			throw new NotImplementedException();
		}

		internal static void GetAsync(Action<WorldAnchorStore> p)
		{
			if(instance){
				//load data if need be and if applicable.
				if(instance.worldAnchorIDs==null){
					instance.worldAnchorIDs = new List<string>();
					if(File.Exists(dataStorePath))
                    	instance.worldAnchorIDs.AddRange(System.IO.File.ReadAllLines(dataStorePath));
                }
			}
			//Application.persistentDataPath
			//save IDs to disk
			//load names 
			
		}
	}
}