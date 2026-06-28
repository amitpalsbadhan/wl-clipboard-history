# Maintainer: Arch Linux User <aur@archlinux.org>
pkgname=wl-clipboard-history
pkgver=1.0.0
pkgrel=1
pkgdesc="A beautiful, high-performance, fully-riceable clipboard history manager for Hyprland using C, GTK4, CSS, and a lightweight bash tracker daemon."
arch=('x86_64' 'aarch64')
url="https://github.com/yourusername/wl-clipboard-history"
license=('MIT')
depends=('gtk4' 'wl-clipboard' 'wtype')
makedepends=('make' 'gcc' 'pkg-config')
source=('Makefile'
        'src/main.c'
        'src/config.c'
        'src/config.h'
        'src/wl-clipboard-history-daemon'
        'config/config.ini'
        'config/style.css')
sha256sums=('SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP')

build() {
  make
}

package() {
  # Install binaries
  install -Dm755 wl-clipboard-history-gui "$pkgdir/usr/bin/wl-clipboard-history-gui"
  install -Dm755 src/wl-clipboard-history-daemon "$pkgdir/usr/bin/wl-clipboard-history-daemon"
  
  # Install default configuration templates
  install -Dm644 config/config.ini "$pkgdir/usr/share/wl-clipboard-history/config.ini"
  install -Dm644 config/style.css "$pkgdir/usr/share/wl-clipboard-history/style.css"
}
