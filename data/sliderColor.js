// Change background color for all range sliders to match the same style
function updateSliderBackground(slider) {
  const min = parseInt(slider.min, 10) || 0;
  const max = parseInt(slider.max, 10) || 100;
  const val = parseInt(slider.value, 10) || 0;
  const percent = ((val - min) / (max - min)) * 100;
  // Green to yellow to gray, same for all sliders
  let color;
  if (percent < 50) {
    color = `linear-gradient(90deg, #00b300 ${percent}%, #e0e0e0 ${percent}%)`;
  } else {
    color = `linear-gradient(90deg, #ffd600 ${percent}%, #e0e0e0 ${percent}%)`;
  }
  slider.style.background = color;
}

document.querySelectorAll('input[type="range"]').forEach(slider => {
  updateSliderBackground(slider);
  slider.addEventListener('input', function() {
    updateSliderBackground(this);
  });
});
