./build.sh

if [ $? -eq 0 ]; then
  clear
  echo "===================================================="
  echo "    Running KeyStone-CLI Release"
  echo "===================================================="
  cd ../../KeyStoneEngine/build/bin/Release/
  ./KeyStone-CLI
else
  echo "Error during compilation"
  exit 1
fi
