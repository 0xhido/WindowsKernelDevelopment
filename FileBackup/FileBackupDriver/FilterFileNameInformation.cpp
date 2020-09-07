#include "FilterFileNameInformation.h"

FilterFileNameInformation::FilterFileNameInformation(PFLT_CALLBACK_DATA data, FileNameOptions options) {
	NTSTATUS ntStatus = FltGetFileNameInformation(data, (FLT_FILE_NAME_OPTIONS)options, &_info);
	if (!NT_SUCCESS(ntStatus)) {
		_info = nullptr;
	}
}

FilterFileNameInformation::~FilterFileNameInformation() {
	if (_info) {
		FltReleaseFileNameInformation(_info);
	}
}

NTSTATUS FilterFileNameInformation::Parse() {
	if (_info) {
		return FltParseFileNameInformation(_info);
	}

	return STATUS_INVALID_PARAMETER;
}