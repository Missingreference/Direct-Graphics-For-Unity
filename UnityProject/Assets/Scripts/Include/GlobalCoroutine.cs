using System;
using System.Collections;
using System.Collections.Generic;

using UnityEngine;

namespace Elanetic.Tools
{
    static public class GlobalCoroutine
    {
        static private GlobalCoroutineExecutor m_Executor;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterAssembliesLoaded)]
        static private void Init()
        {
            m_Executor = new GameObject("Global Coroutine Runner").AddComponent<GlobalCoroutineExecutor>();
            GameObject.DontDestroyOnLoad(m_Executor.gameObject);
            m_Executor.gameObject.hideFlags = HideFlags.HideAndDontSave | HideFlags.HideInInspector;
        }

        static public Coroutine Run(IEnumerator enumerator)
        {
            return m_Executor.StartCoroutine(enumerator);
        }

        static public void Stop(Coroutine coroutine)
        {
#if SAFE_EXECUTION
            if(coroutine == null)
                throw new ArgumentNullException(nameof(coroutine), "Inputted coroutine cannot be null.");
#endif
            m_Executor.StopCoroutine(coroutine);
        }

        [DefaultExecutionOrder(0)]
        private class GlobalCoroutineExecutor : MonoBehaviour
        {
            private bool m_IsQuitting = false;

            void OnDisable()
            {
                if(m_IsQuitting) return;

                throw new InvalidOperationException("GlobalCoroutineExecutor should never be disabled or destroyed during runtime. This will interrupt every coroutine that is running on this Monobehaviour.");
            }

            void OnDestroy()
            {
                if(m_IsQuitting) return;

                throw new InvalidOperationException("GlobalCoroutineExecutor should never be destroyed during runtime. This will interrupt every coroutine that is running on this Monobehaviour.");
            }

            void OnApplicationQuit()
            {
                m_IsQuitting = true;
            }
        }
    }

}