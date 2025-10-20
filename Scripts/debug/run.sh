./build.sh

if [ $? -eq 0 ]; then
  clear
  echo "===================================================="
  echo "    Running KeyStone-CLI Debug"
  echo "===================================================="
  cd ../../KeyStoneEngine/build/bin/Debug/
  ./KeyStone-CLI
else
  echo "Error during compilation"
  exit 1
fi
