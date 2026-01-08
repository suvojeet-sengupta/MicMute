package com.suvmusic.micmute.audio

import com.sun.jna.*
import com.sun.jna.platform.win32.*
import com.sun.jna.platform.win32.COM.COMUtils
import com.sun.jna.platform.win32.WinNT.HRESULT
import com.sun.jna.ptr.PointerByReference

/**
 * Windows Core Audio API bindings via JNA
 * Used to control microphone mute state
 */
object WindowsAudioApi {
    
    // CLSID for MMDeviceEnumerator
    private val CLSID_MMDeviceEnumerator = Guid.CLSID("{BCDE0395-E52F-467C-8E3D-C4579291692E}")
    
    // IID for IMMDeviceEnumerator
    private val IID_IMMDeviceEnumerator = Guid.IID("{A95664D2-9614-4F35-A746-DE8DB63617E6}")
    
    // IID for IAudioEndpointVolume
    private val IID_IAudioEndpointVolume = Guid.IID("{5CDF2C82-841E-4546-9722-0CF74078229A}")
    
    // EDataFlow enum
    const val eCapture = 1 // Microphone
    
    // ERole enum
    const val eConsole = 0
    
    interface Ole32 : Library {
        companion object {
            val INSTANCE: Ole32 = Native.load("Ole32", Ole32::class.java)
        }
        
        fun CoInitializeEx(pvReserved: Pointer?, dwCoInit: Int): HRESULT
        fun CoUninitialize()
        fun CoCreateInstance(
            rclsid: Guid.CLSID,
            pUnkOuter: Pointer?,
            dwClsContext: Int,
            riid: Guid.IID,
            ppv: PointerByReference
        ): HRESULT
    }
    
    fun initializeCOM(): Boolean {
        val hr = Ole32.INSTANCE.CoInitializeEx(null, 0x2) // COINIT_APARTMENTTHREADED
        return hr.intValue() >= 0 || hr.intValue() == 1 // S_OK or S_FALSE (already initialized)
    }
    
    fun uninitializeCOM() {
        Ole32.INSTANCE.CoUninitialize()
    }
    
    fun getDefaultCaptureDevice(): Pointer? {
        val deviceEnumeratorPtr = PointerByReference()
        var hr = Ole32.INSTANCE.CoCreateInstance(
            CLSID_MMDeviceEnumerator,
            null,
            1, // CLSCTX_INPROC_SERVER
            IID_IMMDeviceEnumerator,
            deviceEnumeratorPtr
        )
        
        if (hr.intValue() < 0) return null
        
        val enumeratorVtbl = deviceEnumeratorPtr.value.getPointer(0)
        
        // GetDefaultAudioEndpoint is at index 4 in the vtable
        val getDefaultAudioEndpoint = Function.getFunction(enumeratorVtbl.getPointer(4 * Native.POINTER_SIZE.toLong()))
        
        val devicePtr = PointerByReference()
        val result = getDefaultAudioEndpoint.invokeInt(
            arrayOf(deviceEnumeratorPtr.value, eCapture, eConsole, devicePtr)
        )
        
        return if (result >= 0) devicePtr.value else null
    }
    
    fun getEndpointVolume(device: Pointer): Pointer? {
        val deviceVtbl = device.getPointer(0)
        
        // Activate is at index 3 in the IMMDevice vtable
        val activate = Function.getFunction(deviceVtbl.getPointer(3 * Native.POINTER_SIZE.toLong()))
        
        val volumePtr = PointerByReference()
        val iidBytes = IID_IAudioEndpointVolume.toGuidString()
        
        // We need to pass IID as a structure
        val result = activate.invokeInt(
            arrayOf(device, IID_IAudioEndpointVolume.pointer, 1, Pointer.NULL, volumePtr)
        )
        
        return if (result >= 0) volumePtr.value else null
    }
    
    fun getMute(endpointVolume: Pointer): Boolean {
        val vtbl = endpointVolume.getPointer(0)
        
        // GetMute is at index 8 in IAudioEndpointVolume vtable
        val getMute = Function.getFunction(vtbl.getPointer(8 * Native.POINTER_SIZE.toLong()))
        
        val mutePtr = PointerByReference()
        getMute.invokeInt(arrayOf(endpointVolume, mutePtr))
        
        return mutePtr.value?.getInt(0) != 0
    }
    
    fun setMute(endpointVolume: Pointer, mute: Boolean): Boolean {
        val vtbl = endpointVolume.getPointer(0)
        
        // SetMute is at index 7 in IAudioEndpointVolume vtable
        val setMute = Function.getFunction(vtbl.getPointer(7 * Native.POINTER_SIZE.toLong()))
        
        val result = setMute.invokeInt(
            arrayOf(endpointVolume, if (mute) 1 else 0, Pointer.NULL)
        )
        
        return result >= 0
    }
}
