// The shadow nodes are hand-written, so autolinking is told about their descriptors and their CMake target.
module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: ['FlumeViewComponentDescriptor', 'FlumeSlotViewComponentDescriptor'],
        cmakeListsPath: 'src/main/jni/CMakeLists.txt',
      },
    },
  },
};
