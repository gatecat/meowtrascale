BASE_PATH=$(dirname $(readlink -f "${BASH_SOURCE:-$0}"))
export MEOW_PART="xczu7ev-ffvf1517-2-e"
export MEOW_WORK="${BASE_PATH}/../work/zu7ev"
export MEOW_DEVDB="${BASE_PATH}/../database/xcup/zu7ev"
export PYTHONPATH="${BASE_PATH}/.."
mkdir -p $MEOW_WORK
mkdir -p $MEOW_DEVDB

