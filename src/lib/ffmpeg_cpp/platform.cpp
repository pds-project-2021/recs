#include "platform.h"

#ifdef WIN32

HRESULT enumerateDshowDevices(REFGUID category, IEnumMoniker **ppEnum)
{
	// Create the System Device Enumerator.
	ICreateDevEnum *pDevEnum;
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr,
								  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

	if (SUCCEEDED(hr))
	{
		// Create an enumerator for the category.
		hr = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
		if (hr == S_FALSE)
		{
			hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
		}
		pDevEnum->Release();
	}
	return hr;
}

void getDshowDeviceInformation(IEnumMoniker *pEnum, std::vector<std::string> *audioDevices) {
	IMoniker *pMoniker = nullptr;
	if (audioDevices == nullptr) return;
	while (pEnum->Next(1, &pMoniker, nullptr) == S_OK) {
		IPropertyBag *pPropBag;
		HRESULT hr = pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag));
		if (FAILED(hr)) {
			pMoniker->Release();
			continue;
		}
		VARIANT var;
		VariantInit(&var);
		// Get description or friendly name.
		hr = pPropBag->Read(L"Description", &var, nullptr);
		if (FAILED(hr)) {
			hr = pPropBag->Read(L"FriendlyName", &var, nullptr);
		}
		if (SUCCEEDED(hr)) {
			std::wstring ws(var.bstrVal);// Convert to wstring
			std::string device_name(ws.begin(), ws.end());// Convert to string
			audioDevices->push_back(device_name);// Add to device names vector
			VariantClear(&var);
		}
		pPropBag->Release();
		pMoniker->Release();
	}
}

AVDictionary* get_audio_options(){
	AVDictionary* options = nullptr;
	av_dict_set(&options, "sample_rate", std::to_string(AUDIO_SAMPLE_RATE).c_str(), 0);
	return options;
}

AVDictionary* get_video_options(){
	AVDictionary* options = nullptr;
	av_dict_set(&options, "offset_x", "0", 0);
	return options;
}


std::string get_audio_input_format(){
	return DEFAULT_AUDIO_INPUT_FORMAT;
}

std::string get_audio_input_device(){
	// Set COM to multithreaded model
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	auto audio_devices = std::vector<std::string>();
	if (SUCCEEDED(hr))
	{
		IEnumMoniker *pEnum;

		// Get device enumerator for audio devices
		hr = enumerateDshowDevices(CLSID_AudioInputDeviceCategory, &pEnum);
		if (SUCCEEDED(hr))
		{
			//Get audio devices names
			getDshowDeviceInformation(pEnum, &audio_devices);
			pEnum->Release();
		}
		CoUninitialize();
	}
	else return "";
	// Prepare dshow command input audio device parameter string
	std::string audioInputName;
	audioInputName.append("audio=") ;
	if (audio_devices.empty()) return "";
	auto curr_name = audio_devices.begin();
	for (int i = 0; curr_name != audio_devices.end(); i++) {// Select first available audio device
		if(i==DEFAULT_AUDIO_INPUT_DEVICE) {
            log_debug("Audio input device: " + *curr_name + " selected");
			audioInputName.append(*curr_name);// Write value to string
			break;
		}
		curr_name++;
	}
	return audioInputName;
}

std::string get_video_input_format(){
	return DEFAULT_VIDEO_INPUT_FORMAT;
}

std::string get_video_input_device(const std::string &_offset){
	return DEFAULT_VIDEO_INPUT_DEVICE;
}

#elif defined linux

AVDictionary *get_audio_options() {
	AVDictionary *options = nullptr;
	av_dict_set(&options, "sample_rate", std::to_string(AUDIO_SAMPLE_RATE).c_str(), 0);
	return options;
}

AVDictionary *get_video_options() {
	AVDictionary *options = nullptr;
	av_dict_set(&options, "framerate", "30", 0);
	av_dict_set(&options, "preset", "medium", 0);
	return options;
}

std::string get_audio_input_format() {
	return DEFAULT_AUDIO_INPUT_FORMAT;
}

std::string get_audio_input_device() {
	return DEFAULT_AUDIO_INPUT_DEVICE;
}

std::string get_video_input_format() {
	return DEFAULT_VIDEO_INPUT_FORMAT;
}

std::string get_video_input_device(const std::string &_offset) {
	auto name = DEFAULT_VIDEO_INPUT_DEVICE;
	return std::string(name) + _offset;
}

#else

AVDictionary* get_audio_options(){
	AVDictionary* options = nullptr;
	av_dict_set(&options, "sample_rate", std::to_string(AUDIO_SAMPLE_RATE).c_str(), 0);
	return options;
}

AVDictionary* get_video_options(){
	AVDictionary* options = nullptr;
	av_dict_set(&options, "framerate", "30", 0);
	av_dict_set(&options, "preset", "medium", 0);
	return options;
}

std::string get_audio_input_format(){
	return DEFAULT_AUDIO_INPUT_FORMAT;
}

std::string get_audio_input_device(){
	return DEFAULT_AUDIO_INPUT_DEVICE;
}

std::string get_video_input_format(){
	return DEFAULT_VIDEO_INPUT_FORMAT;
}

std::string get_video_input_device(const std::string &_offset){
	return DEFAULT_VIDEO_INPUT_DEVICE;
}

#endif
