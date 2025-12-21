fetch('widgets/display-overview.html')
    .then(response => response.text())
    .then(text => {
        let e = document.getElementsByClassName('display-overview-panel');
        
        for(let i = 0; i < e.length; i++) {
            e[i].innerHTML = text;
        }
    });