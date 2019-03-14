echo Removing old install
rm -r ~/.local/lib/python3.6/site-packages/fake_spectra*
rm -r build/*
echo Installing new version
python3 setup.py build
python3 setup.py install --user
