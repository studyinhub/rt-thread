function getRandomColor() {
    var letters = '0123456789ABCDEF';
    var color = '#';
    for (var i = 0; i < 6; i++) {
        color += letters[Math.floor(Math.random() * 16)];
    }
    return color;
}

function createStars()
{
    console.log('createStars')
    for (var i = 0; i < 100; i++) {
        var star = document.createElement('div');
        star.className = 'star';
        star.style.top = Math.random() * window.innerHeight + 'px';
        star.style.left = Math.random() * window.innerWidth + 'px';
        star.style.animationDelay = Math.random() * 2 + 's';
        star.style.opacity = Math.random();
        star.style.backgroundColor = getRandomColor()
        document.body.appendChild(star);
    }
}

function createMoon()
{
    var moon = document.createElement('div');
    moon.className = 'moon'
    document.body.appendChild(moon);
}

function createMeteors(){
    var shootingStar = document.createElement('div');
    shootingStar.className = 'shooting-star';
    shootingStar.style.top = Math.random() * window.innerHeight + 'px';
    shootingStar.style.left = Math.random() * window.innerWidth + 'px';
    shootingStar.style.backgroundColor = getRandomColor()
    document.body.appendChild(shootingStar);

    setTimeout(function() {
        document.body.removeChild(shootingStar);
    }, 2500); // Remove the shooting star 2.5 seconds after it's created

}

