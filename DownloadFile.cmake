# DownloadFile.cmake
set(DOWNLOAD_URL "${DOWNLOAD_URL}" CACHE STRING "URL to download")
set(DEST_FILE "${DEST_FILE}" CACHE STRING "File to download to")

file(DOWNLOAD ${DOWNLOAD_URL} ${DEST_FILE}
  STATUS status
  LOG log)

list(GET status 0 status_code)
list(GET status 1 status_string)

if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "Error: Download failed!
    Status: ${status_string}
    Log: ${log}")
endif()