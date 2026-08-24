import React, { useEffect, useState } from 'react';

// Scale-to-fit stage. The pedal is laid out at a fixed design size and the
// whole face scales (preserving aspect) to whatever the host window gives
// us — like resizing a photo of hardware. Nothing can ever be cut off.
export default function FitScale({ designWidth, designHeight, background = '#000', children }) {
  const [scale, setScale] = useState(1);

  useEffect(() => {
    const update = () =>
      setScale(Math.min(window.innerWidth / designWidth, window.innerHeight / designHeight));
    update();
    window.addEventListener('resize', update);
    return () => window.removeEventListener('resize', update);
  }, [designWidth, designHeight]);

  return (
    <div
      style={{
        width: '100vw', height: '100vh', overflow: 'hidden',
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        background
      }}
    >
      <div
        style={{
          width: designWidth, height: designHeight,
          transform: `scale(${scale})`, transformOrigin: 'center center',
          flexShrink: 0
        }}
      >
        {children}
      </div>
    </div>
  );
}
